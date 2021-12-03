//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 3000
#define SEND_DATA "yyyyshen"

void
test_poll_nonblock_conn_client()
{
    //创建连接socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cout << "clientfd create fail" << std::endl;
        return;
    }

    //设置非阻塞模式
    int oldf = fcntl(clientfd, F_GETFL, 0);
    int newf = oldf | O_NONBLOCK;
    if (fcntl(clientfd, F_SETFL, newf) == -1)
    {
        std::cout << "set nonblcok error" << std::endl;
        close(clientfd);
        return;
    }

    //初始化对端地址
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    serveraddr.sin_port = htons(SERVER_PORT);
    //连接
    for (;;)
    {
        int ret = connect(clientfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
        if (ret == 0)
        {
            //直接连接成功
            std::cout << "conn to server succ" << std::endl;
            //只做连接测试，直接结束
            close(clientfd);
            return;
        }
        
        if (ret == -1)
        {
            //未连接成功，可能的情况
            if (errno == EINTR)
            {
                //被中断，重试
                std::cout << "conn interruptted by signal, try again" << std::endl;
                continue;
            }
            if (errno == EINPROGRESS)
            {
                //正在尝试
                std::cout << "waiting for connecting..." << std::endl;
                break;
            }

            //真的出错了
            close(clientfd);
            return;
        }
    }

    //有正在尝试连接的fd，使用poll管理事件
    pollfd pfd_client;
    pfd_client.fd = clientfd;
    pfd_client.events = POLLOUT;
    pfd_client.revents = 0;
    //只有一个，不存容器了
    if (poll(&pfd_client, 1, 3000) != 1)
    {
        //出错了
        close(clientfd);
        std::cout << "[poll] conn to server error" << std::endl;
        return;
    }

    if (!(pfd_client.revents & POLLOUT))
    {
        //事件不是可写
        close(clientfd);
        std::cout << "[poll] conn to server error (not POLLOUT)" << std::endl;
        return;
    }

    //poll成功返回，并且事件可写，在linux下和select一样
    //没连接成功可能也是可写状态，还需要判断socket状态
    int ec;
    socklen_t eclen  = static_cast<socklen_t>(sizeof(ec));
    if (::getsockopt(clientfd, SOL_SOCKET, SO_ERROR, &ec, &eclen) < 0)
    {
        std::cout << "[getsockopt] error" << std::endl;
        close(clientfd);
        return;
    }
   
    if (ec == 0)
        std::cout << "[poll] & [getsockopt] conn to server succ" << std::endl;
    else
        std::cout << "[poll] & [getsockopt] conn to server error" << std::endl;

    close(clientfd);
}

int main(int argc, char* argv[])
{
    test_poll_nonblock_conn_client();

    return 0;
}
