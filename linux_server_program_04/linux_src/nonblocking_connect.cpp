//Copyright(c) by yyyyshen 2021
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#define REMOTE_ADDR "0.0.0.0"
#define REMOTE_PORT 12345
#define BUFFER_SIZE 1023
#define CONN_TIMEOUT 3

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}


void
test_nonblocking_connect()
{
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, REMOTE_ADDR, &remote_addr.sin_addr);
    remote_addr.sin_port = htons(REMOTE_PORT);

    int connfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(connfd >= 0);
    int fdopt = setnonblocking(connfd);
    int ret = connect(connfd, (struct sockaddr*)&remote_addr, sizeof(remote_addr));

    //直接连接成功
    if (ret == 0)
    {
        printf("connect to server immediately\n");
        fcntl(connfd, F_SETFL, fdopt);
        close(connfd);
        return;
    }
    //不是该错误码，则真的出错了
    else if (errno != EINPROGRESS)
    {
        printf("connect failed");
        close(connfd);
        return;
    }

    //使用select检查写事件
    fd_set writefds;
    FD_SET(connfd, &writefds);
    struct timeval timeout;
    timeout.tv_sec = CONN_TIMEOUT;
    timeout.tv_usec = 0;

    ret = select(connfd + 1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0)
    {
        //超时或出错
        printf("connection timeout\n");
        close(connfd);
        return;
    }

    if (!FD_ISSET(connfd, &writefds))
    {
        //返回事件不对
        printf("no write events found\n");
        close(connfd);
        return;
    }

    int error = 0;
    socklen_t length = sizeof(error);
    //获取和清除错误
    if (getsockopt(connfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
    {
        //getsockopt本身调用错误
        close(connfd);
        return;
    }

    if (error != 0)
    {
        //获取的错误不为0，连接出错
        printf("connect failed after select with the error: %d\n", error);
        close(connfd);
        return;
    }

    //连接成功
    printf("connect succeed after select with the socket: %d\n", connfd);
    fcntl(connfd, F_SETFL, fdopt);

    close(connfd);
}

int main(int argc, char* argv[])
{
    test_nonblocking_connect();

    return 0;
}
