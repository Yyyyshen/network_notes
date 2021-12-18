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
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

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

void
echo_server()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    //tcp
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);

    //udp
    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpfd >= 0);
    ret = bind(udpfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    //都管理到epoll下
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    while(true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0)
            break;

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            //tcp监听socket事件
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                //纳入管理
                addfd(epollfd, clifd);
            }
            //udp监听socket事件
            else if (sockfd == udpfd)
            {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);

                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0,
                        (struct sockaddr*)&cli_addr, &cli_addrlen);
                //udp简易的首发
                if (ret > 0)
                    sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0,
                            (struct sockaddr*)&cli_addr, cli_addrlen);
            }
            //正常tcp接收消息
            else if (events[i].events & EPOLLIN)
            {
                char buf[TCP_BUFFER_SIZE];
                while(1)
                {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if (ret < 0)
                    {
                        if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            break;
                        close(sockfd);
                        break;
                    }
                    else if (ret == 0)
                        close(sockfd);
                    else
                        send(sockfd, buf, ret, 0);
                }
            }
            else
            {
                printf("something else happened\n");
            }
        }
    }

    close(listenfd);
    close(udpfd);
}

int main(int argc, char* argv[])
{
    echo_server();

    return 0;
}
