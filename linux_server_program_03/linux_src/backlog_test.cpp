//Copyright(c) by yyyyshen 2021

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5

static bool stop = false;
//SIGTERM信号处理，触发时结束主循环
static void
handle_term(int sig)
{
    stop = true;
}

void
test_backlog()
{
    //创建socket
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    //创建IPv4地址
    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);
    //绑定
    int ret = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    //监听
    ret = listen(sockfd, SERVER_BACKLOG);
    assert(ret != -1);
    //循环等待，直到中断信号到来
    while(!stop)
    {
        sleep(1);
    }
    //回收资源
    close(sockfd);
}

int main(int argc,char* argv[])
{

    test_backlog();

    return 0;
}
