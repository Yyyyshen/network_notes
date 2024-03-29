//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 3000
#define SEND_DATA "helloworld"

void
test_nonblock_conn_client()
{
    //创建socket
    int clientfd = socket(AF_INET,SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        return;
    }
    //设置为非阻塞模式
    int oldf = fcntl(clientfd, F_GETFL, 0);
    int newf = oldf | O_NONBLOCK;
    if(fcntl(clientfd, F_SETFL, newf) == -1)
    {
        close(clientfd);
        return;
    }

    //连接服务器
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    serveraddr.sin_port = htons(SERVER_PORT);

    for(;;)
    {
        int ret = connect(clientfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
        if (ret == 0)
        {
            //直接成功了，只测试连接，所以退出
            std::cout << "conn succ" << std::endl;
            close(clientfd);
            return;
        }
        else if (ret == -1)
        {
            if (errno == EINTR)
                continue;//信号中断重试
            else if (errno == EINPROGRESS)
            {
                std::cout << "connecting..." << std::endl;
                break;//正在尝试，跳出执行下一步用select监听事件
            }
             else
            {
                close(clientfd);
                return;//确实出错了
            }
        }
    }
    
    //尝试连接中，使用select管理可读列表
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(clientfd,&writeset);
    timeval tv;
    tv.tv_sec = 3;//等待3秒
    tv.tv_usec = 0;
    //windows下，一个socket没建立连接之前，使用select检查，结果是不可写
    //即连接成功后才有信号，select可写，以下流程正常
    //if(select(clientfd+1, NULL, &writeset, NULL, &tv) == 1)
    //    std::cout << "(select) conn succ" << std::endl;
    //else
    //    std::cout << "(select) conn fail" << std::endl;
    //但实际在linux下测试，即是没有连接成功，依旧打印了连接成功
    //是因为linux上，没有建立连接之前，select函数检测也是可写的
    //正确逻辑是，不仅判断select是否刻写，还要getsockopt判断socket是否出错
    //通过错误码确认是否连接成功
    //如下
    if(select(clientfd +1, NULL, &writeset, NULL, &tv) != 1)
    {
        std::cout << "(select) conn err" << std::endl;
        close(clientfd);
        return;
    }
    
    int ec;
    socklen_t ec_len = static_cast<socklen_t>(sizeof(ec));
    if(::getsockopt(clientfd, SOL_SOCKET, SO_ERROR, &ec, &ec_len) < 0)
    {
        close(clientfd);
        return;
    }
    if(ec == 0)
        std::cout << "(select)&(getsockopt) conn succ" << std::endl;
    else
        std::cout << "(select)&(getsockopt) conn fail" << std::endl;
    
    //资源回收
    close(clientfd);

}

int main(int argc, char* argv[])
{
    test_nonblock_conn_client();

    return 0;
}
