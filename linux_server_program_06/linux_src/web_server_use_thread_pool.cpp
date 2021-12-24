//Copyright(c) by yyyyshen 2021
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

//自定义类
#include "locker.hpp"
#include "thread_pool.hpp"
#include "http_conn.hpp"

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 50000
#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000

//extern int addfd(int epollfd, int fd, bool one_shot);
//extern int removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[])
{
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池
    thread_pool<http_conn>* pool = nullptr;
    try
    {
        pool = new thread_pool<http_conn>;
    }
    catch(...)
    {
        return 1;
    }

    //为每个连接分配一个对象，用fd值作索引
    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    //epoll管理
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    //主循环
    while (true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
            break;

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            //监听新连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                if (connfd < 0)
                    continue;
                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                //初始化客户连接
                users[connfd].init(connfd, cli_addr);
            }
            //异常
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                users[sockfd].close_conn();
            else if (events[i].events & EPOLLIN)
            {
                //由主线程操作读
                //根据结果决定投入任务到池还是退出
                if (users[sockfd].read())
                    //任务被接收后，此socket若请求处理完整、正确，则切换为写状态
                    pool->append(users+sockfd);
                else
                    users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLOUT)
            {
                //任务被正确处理后的连接会切到写状态，进行一次响应
                if (!users[sockfd].write())
                    users[sockfd].close_conn();//写失败或不是keep-alive，关闭连接
                //响应一次结束后，会重新注册成读状态，继续处理下一个请求
            }
            else
            {}
        }
    }

    //回收资源
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
