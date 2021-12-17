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

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5

void
test_select()
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
    {
        close(listenfd);
        return;
    }

    //初始化事件列表
    char buf[1024];
    fd_set read_fds;
    fd_set excp_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&excp_fds);

    //服务器主循环中使用select
    while(1)
    {
        memset(buf, '\0', sizeof(buf));
        //每次调用前都需要重新设置set
        //因为事件发生后，fdset将被内核修改
        FD_SET(clifd, &read_fds);
        FD_SET(clifd, &excp_fds);
        ret = select(clifd + 1, &read_fds, NULL, &excp_fds, NULL);
        if (ret < 0)
            break;

        //检测到可读事件
        if (FD_ISSET(clifd, &read_fds))
        {
            ret = recv(clifd, buf, sizeof(buf) - 1, 0);
            if (ret <= 0)
                break;

            printf("get %d bytes of normal data: %s\n", ret, buf);
        }
        else if (FD_ISSET(clifd, &excp_fds))
        {
            ret = recv(clifd, buf, sizeof(buf) - 1, MSG_OOB);
            if (ret <= 0)
                break;

            printf("get %d bytes of oob data: %s\n", ret, buf);
        }
    }

    close(clifd);
    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_select();

    return 0;
}
