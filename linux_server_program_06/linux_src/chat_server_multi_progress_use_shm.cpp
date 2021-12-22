//Copyright(c) by yyyyshen 2021
//

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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5
#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65535

//单个客户连接使用的数据结构
struct client_data
{
    sockaddr_in address;//socket地址
    int connfd;         //文件描述符
    pid_t pid;          //子进程pid
    int pipefd[2];      //用于与父进程通信的管道
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char* share_mem = 0;//共享内存
client_data* users = 0;//客户端连接组，用客户连接编号索引，取得客户连接数据
int* sub_process = 0;//子进程和客户连接映射关系表，用进程pid做索引，取得客户连接编号
int user_count = 0;
bool stop_child = false;

//一些通用函数
int setreuseaddr(int fd)
{
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
}

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void(*handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//资源回收
void del_resource()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete[] users;
    delete[] sub_process;
}

//停止一个子进程
void child_term_handler(int sig)
{
    stop_child = true;
}

//子进程运行函数
//idx为客户连接编号，users为保存所有连接数据的数组，share_mem为共享内存地址
int run_child(int idx, client_data* users, char* share_mem)
{
    //子进程同样用epoll的I/O复用机制
    epoll_event events[MAX_EVENT_NUMBER];
    //监听两个fd，一个是与客户端的连接socket，一个是与父进程通信管道
    int child_epollfd = epoll_create(1);
    assert(child_epollfd != -1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);

    //子进程设置自己的信号处理
    addsig(SIGTERM, child_term_handler,false);//这里只关注自己关闭的信号，用于父进程结束自己

    int ret = 0;
    //子进程循环
    while (!stop_child)
    {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
            break;

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;

            //客户连接逻辑处理
            if ((sockfd == connfd) && (events[i].events & EPOLLIN))
            {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                //不需要处理同步，因为每个客户分别处理自己那一块内存，不涉及竞争
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if (ret < 0)
                {    
                    if (errno != EAGAIN)
                        stop_child = true;
                }
                else if (ret == 0)
                    stop_child = true;
                else
                    //通知主进程自己收到数据了
                    send(pipefd, (char*)&idx, sizeof(idx), 0);
            }
            //负责接收来自父进程的管道消息
            else if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client = 0;
                //先从父进程收到哪个客户端连接发来了数据
                ret = recv(pipefd, (char*)&client, sizeof(client), 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                        stop_child = true;
                }
                else if (ret == 0)
                    stop_child = true;
                else
                    //根据编号，将对应共享数据块的内容发给客户端
                    //同样不需要处理同步，因为只是读操作
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
            }
            else
                continue;//其他情况，不管
        }
    }
    //回收子进程自己的资源
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}

//主循环
void
chat_server()
{
    //开启监听服务
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = setreuseaddr(listenfd);
    assert(ret != -1);
    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    //初始化用户相关变量
    user_count = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for(int i = 0; i < PROCESS_LIMIT; ++i)
        sub_process[i] = -1;

    //epoll管理部分
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(1);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    //信号源统一，管道通知
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);//非阻塞管道写
    addfd(epollfd, sig_pipefd[0]);//注册管道读
    //关注信号类型
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);//向关闭的客户端写时触发，忽略防止进程被系统结束
    bool stop_server = false;
    bool terminate = false;

    //共享内存，作为所有socket上的读缓存
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    assert(ret != -1);
    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE,
            PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    //服务器主循环
    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR))
            break;//返回-1且不是被信号中断的，确实失败了

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            //监听fd的读事件，有新连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                if (connfd < 0)
                    continue;//接收失败
                //连接满了
                if (user_count >= USER_LIMIT)
                {
                    const char* info = "too many users\n";
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                //成功接收连接，保存相关数据
                users[user_count].address = cli_addr;
                users[user_count].connfd = connfd;
                //建立父子进程间的通信管道
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                //创建子进程
                pid_t pid = fork();
                if (pid < 0)
                {
                    close(connfd);
                    continue;//创建失败
                }
                else if (pid == 0)
                {
                    //子进程
                    //关闭不必要的资源
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    //执行子进程循环
                    run_child(user_count, users, share_mem);
                    //子进程循环结束，回收资源并关闭
                    munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }
                else
                {
                    //父进程
                    //关闭多余资源
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    //将用户进程间通信的管道纳入epoll管理
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;//记录进程pid和客户编号的映射
                    ++user_count;
                }
            }
            //处理信号事件
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                    continue;
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD://子进程退出
                                {
                                    pid_t pid;
                                    int stat;
                                    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                    {
                                        //通过子进程pid，取得客户连接编号
                                        int del_user = sub_process[pid];
                                        sub_process[pid] = -1;
                                        if ((del_user < 0) || (del_user > USER_LIMIT))
                                            continue;
                                        //清除对应客户的数据
                                        epoll_ctl(epollfd, EPOLL_CTL_DEL,
                                                users[del_user].pipefd[0], 0);
                                        close(users[del_user].pipefd[0]);
                                        //将结尾的用户交换过来
                                        users[del_user] = users[--user_count];
                                        sub_process[users[del_user].pid] = del_user;
                                    }
                                    //父进程标记结束，子进程也全部结束，则关闭程序
                                    if (terminate && user_count == 0)
                                        stop_server = true;
                                    break;
                                }
                            case SIGTERM:
                            case SIGINT:
                                {
                                    //结束信号，关闭服务
                                    printf("kill all child first\n");
                                    if (user_count == 0)
                                    {
                                        stop_server = true;
                                        break;
                                    }
                                    for (int i = 0; i < user_count; ++i)
                                    {
                                        int pid = users[i].pid;
                                        kill(pid, SIGTERM);
                                    }
                                    terminate = true;
                                    break;
                                }
                            default:
                                break;
                        }
                    }
                }
            }
            //某子进程（客户连接）向父进程写入了数据
            else if (events[i].events & EPOLLIN)
            {
                int child = 0;
                //进程间通信，子进程只告诉父进程哪个连接有数据到达
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                if (ret == -1)
                    continue;
                else if (ret == 0)
                    continue;
                else
                {
                    //由父进程通知除此子进程外其他子进程，把共享内存读缓存中的数据写出去
                    for (int j = 0; j < user_count; ++j)
                    {
                        if (users[j].pipefd[0] == sockfd)
                            continue;//跳过此子进程

                        printf("send data to child accross pipe\n");
                        send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
                    }
                }
            }
        }
    }

    //清理资源
    del_resource();
}

int main(int argc, char* argv[])
{
    chat_server();

    return 0;
}
