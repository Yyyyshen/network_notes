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
#include <pthread.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

//各函数声明
int
setnonblocking( //设置描述符为非阻塞
        int fd
        );

void
addfd(  //将指定fd关注的事件注册到epoll内核事件表
        int epollfd,
        int fd,
        bool enable_at  //是否启用ET模式
     );

void
lt( //LT模式工作流程
        epoll_event* events,
        int number,
        int epollfd,
        int listenfd
  );

void
et( //ET模式工作流程
        epoll_event* events,
        int number,
        int epollfd,
        int listenfd
  );

//服务器逻辑
void
test_epoll_lt_and_et()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);

    //epoll用法
    //事件数组，用于接收内核返回的就绪事件数组
    epoll_event events[MAX_EVENT_NUMBER];
    //内核时间表
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    //将监听fd加入关注事件数组
    addfd(epollfd, listenfd, true);

    while(true)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
            break;

        //lt(events, ret, epollfd, listenfd);
        et(events, ret, epollfd, listenfd);
    }

    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_epoll_lt_and_et();

    return 0;
}

//各函数定义
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;//关注读操作
    if (enable_et)
        event.events |= EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void lt(epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; ++i)
    {
        int sockfd = events[i].data.fd;
        //若是监听描述符
        if(sockfd == listenfd)
        {
            struct sockaddr_in cli_addr;
            socklen_t cli_addrlen = sizeof(cli_addr);
            int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
            //将接收到的新连接加入内核事件表中
            addfd(epollfd, clifd, false);
        }
        //普通连接描述符可读事件发生
        else if (events[i].events & EPOLLIN)
        {
            printf("event EPOLLIN trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if(ret <= 0)
                continue;
            printf("get %d bytes of content: %s\n", ret, buf);
        }
        else
            printf("something else happened\n");
    }
}

void et(epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; ++i)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in cli_addr;
            socklen_t cli_addrlen = sizeof(cli_addr);
            int clifd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
            addfd(epollfd, clifd, true);//使用ET模式
        }
        else if (events[i].events & EPOLLIN)
        {
            printf("event EPOLLIN trigger once\n");
            //不会重复触发，所以需要完全读完
            while (true)
            {
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret < 0)    //非阻塞模式下ret小于0不一定是失败
            {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                {
                    //非阻塞I/O下，数据读取完毕，可以继续监听下一次读事件发生了
                    printf("read later\n");
                    break;
                }
                //如果不是这两个，则就是真的出错了
                close(sockfd);
                break;
            }
            else if (ret == 0)  //对端关闭了连接
                close(sockfd);
            else
                printf("get %d bytes of content: %s\n", ret, buf);
            }
        }
        else
            printf("something else happended\n");
    }
}
