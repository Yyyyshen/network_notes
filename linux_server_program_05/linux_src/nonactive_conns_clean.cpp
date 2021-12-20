//Copyright(c) by yyyyshen 2021
//

#include "chapter_05_public.h"
#include "timer_based_on_linkedlist.hpp"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

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
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时器处理
void timer_handler()
{
    //定时器处理实际上是调用tick
    timer_lst.tick();
    //每次alarm只会触发一次信号，所以要重新定时，不断触发
    alarm(TIMESLOT);
}

//定时器回调，删除非活动连接的注册事件并关闭
void cb_func(client_data* user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

void 
test_clean_nonactive_conns()
{
    //网络监听部分
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int ret = 0;
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    ret = setreuseaddr(listenfd);
    assert(ret != -1);
    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    //epoll管理部分
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    //管道用于发送信号
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    //给关注的信号添加处理函数
    addsig(SIGALRM);
    addsig(SIGTERM);

    bool stop_server = false;

    //空间换时间，fd值直接对应数据索引位置，做到随机访问
    client_data* users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);//开始第一次定时，后续由触发后的处理函数再次定时

    while(!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR))
            break;//epoll_wait失败

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            //监听fd，有新连接进来
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                addfd(epollfd, connfd);
                //设置数据属性
                users[connfd].address = cli_addr;
                users[connfd].sockfd = connfd;
                //定时器创建，设置回调和超时时间，绑定后添加到链表
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);//当前时间
                timer->expire = cur + 3 * TIMESLOT;//超过三次定时，则认为不活跃
                users[connfd].timer = timer;
                timer_lst.add_timer(timer);
            }
            //信号处理
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                    continue;
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:
                                {
                                    //仅作标记，但不立即处理，要优先I/O事件处理
                                    timeout = true;
                                    break;
                                }
                            case SIGTERM:
                                {
                                    stop_server = true;
                                    break;
                                }
                        }
                    }
                }
            }
            //客户连接上的数据
            else if (events[i].events & EPOLLIN)
            {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d\n",
                        ret, users[sockfd].buf, sockfd);

                //当前fd绑定的定时器
                util_timer* timer = users[sockfd].timer;

                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        //不是这个errno的话，就是出错了，关闭连接，删除定时器
                        cb_func(&users[sockfd]);
                        if (timer)
                            timer_lst.del_timer(timer);
                    }
                }
                else if (ret == 0)
                {
                    //对端关闭连接，那么我们也关闭，并删除定时器
                    cb_func(&users[sockfd]);
                    if (timer)
                            timer_lst.del_timer(timer);
                }
                else
                {
                    //可读数据有效，则说明连接活跃，重置超时时间
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    printf("conn active, reset timer\n");
                    timer_lst.adjust_timer(timer);
                }
            }
            else
            {
                //其他事件
            }
        }

        //定时事件放在最后处理，因为I/O应有更高优先级，但这也导致定时任务不够精确
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }

    //回收资源
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
}

int main(int argc, char* argv[])
{
    test_clean_nonactive_conns();

    return 0;
}
