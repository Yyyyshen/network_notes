#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//不断向服务器发送此请求
static const char* request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxx";

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void close_conn(int epollfd, int sockfd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

//向服务器写数据
bool write_nbytes(int sockfd, const char* buffer, int len)
{
    int bytes_write = 0;
    while (1)
    {
        bytes_write = send(sockfd, buffer, len, 0);
        if (bytes_write == -1)
            return false;
        else if (bytes_write == 0)
            return false;
        
        len -=bytes_write;
        buffer = buffer + bytes_write;
        if (len <= 0)
            return true;
    }
}

//从服务器读数据
bool read_once(int sockfd, char*buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(sockfd, buffer, len, 0);
    if (bytes_read == -1 || bytes_read == 0)
        return false;

    return true;
}

//向服务器发起num个TCP连接，通过调整num调整测试压力
void start_conn(int epollfd, int num, const char* ip, int port)
{
    int ret = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htons(port);

    for (int i = 0; i < num; ++i)
    {
        sleep(1);
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
            addfd(epollfd, sockfd);
    }
}

int main(int argc, char* argv[])
{
    assert(argc == 4);
    int epollfd = epoll_create(1);
    start_conn(epollfd, atoi(argv[3]), argv[1], atoi(argv[2]));
    epoll_event events[10000];
    char buffer[2048];
    while(1)
    {
        int fds = epoll_wait(epollfd, events, 10000, 2000);
        for (int i = 0; i < fds; ++i)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (!read_once(sockfd, buffer, 2048))
                    close_conn(epollfd, sockfd);
                //接着发
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!write_nbytes(sockfd, request, strlen(request)))
                    close_conn(epollfd, sockfd);
                //发完切到读事件读一次
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLERR)
                close_conn(epollfd, sockfd);
        }
    }
}
