//Copyright(c) by yyyyshen 2021
//

#include "chapter_05_public.h"

#define CONN_TIMEOUT 3

int 
timeout_connect()
{
    int ret = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    //通过选项SO_SNDTIMEO设置的超时类型是timeval，与I/O复用里的相同
    struct timeval timeout;
    timeout.tv_sec = CONN_TIMEOUT;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);

    ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1)
    {
        //返回-1并且错误码如下则说明超时
        if (errno == EINPROGRESS)
        {
            printf("connecting timeout, process timeout logic\n");
            return -1;
        }
        printf("errno occur when connecting to server\n");
        return -1;
    }

    return sockfd;
}

int main(int argc, char* argv[])
{
    int sockfd = timeout_connect();
    if (sockfd < 0)
        return 1;

    return 0;
}
