//Copyright(c) by yyyyshen 2021
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5

void
test_echo_use_splice()
{
    //开启服务
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);
    //接收连接
    struct sockaddr_in cli_addr;
    socklen_t cli_addrlen  = sizeof(cli_addr);
    int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
    if (clifd < 0)
        printf("accept failed, errno: %d\n", errno);
    else
    {
        //回显，利用内核操作splice实现零拷贝
        int pipefd[2];
        ret = pipe(pipefd);//创建管道
        assert(ret != -1);
        ret = splice(clifd, NULL, pipefd[1], NULL, 32768,//管道大小对半分
                SPLICE_F_MORE | SPLICE_F_MOVE);//将socket上的输入定向到管道写
        assert(ret != -1);
        ret = splice(pipefd[0], NULL, clifd, NULL, 32768,
                SPLICE_F_MORE | SPLICE_F_MOVE);//将管道读定向到socket输出
        assert(ret != -1);
        //整个流程不涉及recv/send，所以没有用户和内核空间数据拷贝
        close(clifd);
    }
}

int main(int argc, char* argv[])
{
    test_echo_use_splice();

    return 0;
}
