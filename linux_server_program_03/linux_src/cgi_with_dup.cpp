//Copyright(c) by yyyyshen 2021

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5

void
test_cgi_with_dup()
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

    struct sockaddr_in cli_addr;
    socklen_t cli_addrlen = sizeof(cli_addr);
    int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
    if (clifd < 0)
        printf("accept failed, errno: %d\n",errno);
    else
    {
        //关闭标准输出
        close(STDOUT_FILENO);
        //复制客户端连接socket的fd，而由于dup返回的是系统中最小的可用文件描述符，所以返回的是刚关闭的标准输出（值1）
        dup(clifd);
        //此时，使用printf，原本是使用标准输出，现在被输出到了连接的文件描述符中，也就是重定向到了socket上
        printf("gotcha\n");
        close(clifd);
    }

    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_cgi_with_dup();

    return 0;
}
