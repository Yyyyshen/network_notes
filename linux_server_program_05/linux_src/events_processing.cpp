//Copyright(c) by yyyyshen 2021
//

#include "chapter_05_public.h"

#define MAX_EVENT_NUMBER 1024

static int pipefd[2];

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void sig_handler(int sig)
{
    //记录原有errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);//向管道写信号值，以通知主循环
    errno = save_errno;//因为有函数操作可能修改错误值，所以要恢复，保证函数可重入性
}

//设置信号处理函数
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    //将关注的信号的处理函数设置为自定义函数
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void 
test_events_process()
{
    //网络部分
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = SERVER_PORT;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = setreuseaddr(listenfd);
    assert(ret != -1);
    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    //创建管道，用于信号接收
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);//非阻塞写管道
    addfd(epollfd, pipefd[0]);//将读管道的事件进行监听

    //设置信号处理函数，表示关注哪些信号
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);

    //主循环
    bool stop_server = false;
    while(!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;//之前管道用的是socketpair，统称sockfd没问题

            //监听fd，处理新连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                addfd(epollfd, clifd);
            }
            //管道fd可读，处理信号
            else if (sockfd == pipefd[0] && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                    continue;
                else
                {
                    //每个信号值1字节，逐个接收
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                            //简单处理下，以安全中止服务循环为例
                            case SIGCHLD:
                            case SIGHUP:
                                continue;
                            case SIGTERM:
                            case SIGINT:
                                stop_server = true;
                        }
                    }
                }
            }
            //其他的，正常连接读写事件等
            else
            {
                //略
            }
        }
    }

    //资源清理
    printf("server stop\n");
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
}

int main(int argc, char* argv[])
{
    test_events_process();

    return 0;
}
