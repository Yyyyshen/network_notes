//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

void
test_server()
{
    //监听socket
    int listenfd = socket(AF_INET, SOCK_STREAM,0);
    if (listenfd == -1)
    {
        std::cout << "create listen socket failed" << std::endl;
        return;
    }

    //初始化地址二元组
    sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(3000);
    //绑定监听socket与地址
    if (bind(listenfd, (sockaddr *)&bindaddr,sizeof(bindaddr)) == -1)
    {
        std::cout << "bind listen socket failed" << std::endl;
        return;
    }

    //启动监听
    if (listen(listenfd,SOMAXCONN) == -1)
    {
        std::cout << "start listen failed" << std::endl;
        return;
    }

    //循环接收客户端连接
    while (true)
    {
        sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int clientfd = accept(listenfd,(sockaddr*)&clientaddr, &clientaddrlen);
        if(clientfd != -1)
        {
            char recvBuf[1024] = {0};
            //接收数据
            int ret = recv(clientfd, recvBuf, 1024, 0);
            if (ret > 0)
            {
                std::cout << "recv data from client, data:" << recvBuf << std::endl;
                //回显
                ret = send(clientfd, recvBuf, strlen(recvBuf), 0);
                if (ret != strlen(recvBuf))
                    std::cout << "send data error" << std::endl;
                else
                    std::cout << "send data to client successfully" << std::endl;
            }
            else
            {
                std::cout << "recv data error" << std::endl;
            }

            //关闭本次连接
            close(clientfd);
        }
    }

    //关闭监听socket
    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_server();

    return 0;
}
