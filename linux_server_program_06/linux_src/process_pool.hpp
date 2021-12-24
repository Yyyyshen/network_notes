#ifndef PROCESS_POLL_HPP
#define PROCESS_POLL_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

//一个子进程
class process
{
public:
    process() : m_pid(-1) {}
public:
    pid_t m_pid;
    int m_pipefd[2];//与父进程通信的管道
};

//池类
//模板类，参数是处理逻辑任务的类
template<typename T>
class process_pool
{
private:
    //私有构造，需要单例
    process_pool(int listenfd, int process_number = 8);
public:
    //确保只有一个实例
    static process_pool<T>* create(int listenfd, int process_number = 8)
    {
        if (m_instance == nullptr)
            m_instance = new process_pool<T>(listenfd, process_number);
        return m_instance;
    }
    ~process_pool()
    {
        delete[] m_sub_process;
    }
    //启动
    void run();

private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    //最大允许子进程数量
    static const int MAX_PROCESS_NUMBER = 16;
    //每个子进程最多处理的客户数量
    static const int USER_PER_PROCESS = 65535;
    //epoll能处理的最大事件数
    static const int MAX_EVENT_NUMBER = 10000;
    //池内进程数
    int m_process_number;
    //子进程在池中序号
    int m_idx;
    //每个进程都有一个epoll事件表
    int m_epollfd;
    //监听socket
    int m_listenfd;
    //停止运行标记
    int m_stop;
    //保存子进程描述信息
    process* m_sub_process;
    //单个实例
    static process_pool<T>* m_instance;
};

template<typename T>
process_pool<T>* process_pool<T>::m_instance = nullptr;

//处理信号的管道
static int sig_pipefd[2];

//static将函数限定在声明的文件内
static int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno - save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//构造，其中listenfd必须在创建进程池之前创建，否则子进程无法直接引用
template<typename T>
process_pool<T>::process_pool(int listenfd, int process_number)
    : m_listenfd(listenfd),
    m_process_number(process_number),
    m_idx(-1),
    m_stop(false)
{
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
    m_sub_process = new process[process_number];
    assert(m_sub_process);

    //创建子进程，并建立管道
    for (int i = 0; i < process_number; ++i)
    {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);
        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if (m_sub_process[i].m_pid > 0)
        {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

//统一事件源
template<typename T>
void process_pool<T>::setup_sig_pipe()
{
    //创建epoll监听和管道
    m_epollfd = epoll_create(1);
    assert(m_epollfd != -1);
    
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    //设置关注的信号
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

//根据index判断运行父进程代码还是子进程代码
template<typename T>
void process_pool<T>::run()
{
    if (m_idx != -1)
    {
        run_child();
        return;
    }
    run_parent();
}

template<typename T>
void process_pool<T>::run_child()
{
    setup_sig_pipe();

    //根据自己的index获取自身各属性
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    //加入epoll管理，以便接收父进程的通知，有新连接到来
    addfd(m_epollfd, pipefd);

    //提前声明好用到的变量
    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];//将每个用户数据存起来，编号根据fd值，空间换时间
    assert(users);
    int number = 0;//epoll事件数量
    int ret = -1;//各函数操作结果

    //子进程主循环
    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
            break;

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            //与父进程通信的管道接收到数据，接收新客户连接
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client = 0;
                //读父进程发来的数据
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0)
                    continue;
                else
                {
                    //读取成功，说明有新连接，接收
                    struct sockaddr_in cli_addr;
                    socklen_t cli_addrlen = sizeof(cli_addr);
                    int connfd = accept(m_listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                    if (connfd < 0)
                        continue;

                    //接收成功，纳入epoll管理
                    addfd(m_epollfd, connfd);
                    //初始化一个新客户数据，模板类参数要实现指定方法
                    users[connfd].init(m_epollfd, connfd, cli_addr);
                }
            }
            //子进程收到的信号处理
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                    continue;
                else
                {
                    //逐个处理信号
                    for (int i = 0; i < ret; ++i)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD:
                                {
                                    pid_t pid;
                                    int stat;
                                    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                        continue;
                                    break;
                                }
                            case SIGTERM:
                            case SIGINT:
                                {
                                    m_stop = true;
                                    break;
                                }
                            default:
                                break;
                        }
                    }
                }
            }
            //其他的读事件就是客户连接发来的了
            else if (events[i].events & EPOLLIN)
            {
                //使用user自身的处理函数处理业务
                users[sockfd].process();
            }
            //其他事件不管
            else
            {
                continue;
            }
        }
    }

    //子进程循环结束，回收资源
    delete[] users;
    users = nullptr;
    close(pipefd);
    close(m_epollfd);
}

template<typename T>
void process_pool<T>::run_parent()
{
    setup_sig_pipe();

    //父进程监听socket逻辑
    addfd(m_epollfd, m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;//用于轮询分配子进程
    int new_conn = 1;//有新连接进来的标记，用于通知子进程
    int number = 0;//epoll事件数
    int ret = -1;

    //父进程主循环
    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
            break;

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            //新连接到来
            if (sockfd == m_listenfd)
            {
                //轮询方式通知一个子进程来接收
                int i = sub_process_counter;
                do 
                {
                    if (m_sub_process[i].m_pid != -1)
                        break;
                    i = (i + 1) % m_process_number;
                }
                while (i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1)
                {
                    m_stop = true;
                    break;//没有可用子进程，退出
                }

                sub_process_counter = (i + 1) % m_process_number;//记录下一个轮询位置
                //通知对应子进程
                send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
            }
            //父进程信号处理
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                    continue;
                else
                {
                    //逐个处理信号
                    for (int i = 0; i < ret; ++i)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD:
                                {
                                    pid_t pid;
                                    int stat;
                                    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                    {
                                        //处理退出的子进程
                                        for (int i = 0; i < m_process_number; ++i)
                                        {
                                            if (m_sub_process[i].m_pid == pid)
                                            {
                                                //关闭通信管道，pid置-1，表示退出
                                                close(m_sub_process[i].m_pipefd[0]);
                                                m_sub_process[i].m_pid = -1;
                                            }
                                        }
                                    }
                                    //检查子进程，如果都退出了，父进程退出
                                    m_stop = true;
                                    for (int i = 0; i < m_process_number; ++i)
                                        if (m_sub_process[i].m_pid != -1)
                                            m_stop = false;

                                    break;
                                }
                            case SIGTERM:
                            case SIGINT:
                                {
                                    //父进程收到终止信号，杀掉所有子进程
                                    for (int i = 0; i < m_process_number; ++i)
                                    {
                                        int pid = m_sub_process[i].m_pid;
                                        if (pid != -1)
                                            kill(pid, SIGTERM);
                                        //也可以使用管道自定义消息通知其关闭
                                    }
                                    break;
                                }
                            default:
                                break;
                        }
                    }
                }
            }
            //其他事件不管
            else
            {
                continue;
            }
        }
    }

    //listenfd由主函数创建和关闭
    close(m_epollfd);
}

#endif//!PROCESS_POLL_HPP
