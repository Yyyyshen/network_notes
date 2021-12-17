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
#define BUFFER_SIZE 1024

struct fds
{
    int epollfd;
    int sockfd;
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool oneshot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//再次设置oneshot
void reset_oneshot(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//工作线程
void* worker(void* arg)
{
    int sockfd = ((fds*)arg)->sockfd;
    int epollfd = ((fds*)arg)->epollfd;
    printf("new thread start to recv data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    //读完数据
    while(true)
    {
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0)
        {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }
        else if (ret < 0)
        {
            //读完的情况，重置事件
            if (errno == EAGAIN)
            {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else
        {
            printf("get content: %s\n", buf);
            sleep(5);//模拟数据处理过程
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
}

void
test_epoll_oneshot()
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
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    //监听fd不能使用oneshot，否则就只能处理一个客户连接了
    addfd(epollfd, listenfd, false);

    while(true)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
            break;

        for (int i = 0; i < ret; ++i)
        {
            int sockfd = events[i].data.fd;
            //监听事件触发
            if (sockfd == listenfd)
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
                //将接收到的连接加入事件表，启用oneshot
                addfd(epollfd, clifd, true);
            }
            //普通客户连接触发读事件
            else if (events[i].events & EPOLLIN)
            {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                //启用新的线程服务此次事件
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            }
            else
                printf("something else happened\n");
        }
    }

    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_epoll_oneshot();

    return 0;
}
