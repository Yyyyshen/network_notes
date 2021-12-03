//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <errno.h>

//无效fd标记
#define INVALID_FD -1
#define SERVER_PORT 3000

void
test_poll_server()
{
    //创建监听socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == INVALID_FD)
    {
        std::cout << "listenfd create fail" << std::endl;
        return;
    }

    //设置为非阻塞
    int oldf = fcntl(listenfd, F_GETFL, 0);
    int newf = oldf | O_NONBLOCK;
    if (fcntl(listenfd, F_GETFL, newf) == -1)
    {
        close(listenfd);
        std::cout << "set listenfd nonblock fail, close socket" << std::endl;
        return;
    }

    //复用地址和端口
    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char*)&on, sizeof(on));

    //初始化服务器地址二元组
    sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(SERVER_PORT);
    //绑定
    if (bind(listenfd, (sockaddr*)&bindaddr, sizeof(bindaddr)) == -1)
    {
        std::cout << "bind listen socket fail" << std::endl;
        close(listenfd);
        return;
    }

    //开始监听
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        std::cout << "listen error" << std::endl;
        close(listenfd);
        return;
    }

    //初始化poll列表
    std::vector<pollfd> vec_pfds;
    pollfd pfd_listen;
    pfd_listen.fd = listenfd;
    pfd_listen.events = POLLIN;
    pfd_listen.revents = 0;
    vec_pfds.push_back(pfd_listen);

    //是否存在无效fd标志
    bool exist_invalid_fd;
    int n;
    while (true)
    {
        exist_invalid_fd = false;
        //传入首地址和组大小，超时单位毫秒
        n = poll(&vec_pfds[0], vec_pfds.size(), 1000);
        if (n < 0)
        {
            //被中断则重试
            if (errno == EINTR)
                continue;
            //否则出错退出
            break;
        }
        else if (n == 0)
        {
            //超时，重试
            continue;
        }

        //有事件发生，和select一样需要遍历
        for (size_t i = 0; i < vec_pfds.size(); ++i)
        {
            //检测是否为可读事件
            if (vec_pfds[i].revents & POLLIN)
            {
                //判断是否为监听socket
                if (vec_pfds[i].fd == listenfd)
                {
                    //有新连接进来
                    sockaddr_in clientaddr;
                    socklen_t clientaddrlen = sizeof(clientaddr);
                    int clientfd = accept(listenfd, (sockaddr*)&clientaddr, &clientaddrlen);
                    
                    if (clientfd == INVALID_FD)
                        continue;

                    //设置为非阻塞
                    int oldf = fcntl(clientfd, F_GETFL, 0);
                    int newf = oldf | O_NONBLOCK;
                    if (fcntl(clientfd, F_SETFL, newf) == -1)
                    {
                        close(clientfd);
                        continue;
                    }

                    //将新连接加入poll列表
                    pollfd pfd_client;
                    pfd_client.fd = clientfd;
                    pfd_client.events = POLLIN;
                    pfd_client.revents = 0;
                    vec_pfds.push_back(pfd_client);
                    std::cout << "accept new client, fd: " << clientfd << std::endl;
                }
                else
                {
                    //clientfd，表示接收数据
                    char buf[1024] = {0};
                    int ret = recv(vec_pfds[i].fd, buf, 1024, 0);
                    if (ret <= 0)
                    {
                        //没获取到数据
                        if (errno != EINTR && errno != EWOULDBLOCK)
                        {
                            //走到这里确认是出错或对端关闭连接，关闭fd，设置无效
                            std::cout << "client disconnected, clientfd: "
                                << vec_pfds[i].fd << std::endl;
                            //(这里书中的例子又诡异的遍历一遍vec然后对比fd是否相等，迷惑)
                            close(vec_pfds[i].fd);
                            vec_pfds[i].fd = INVALID_FD;
                            exist_invalid_fd = true;
                        }
                    }
                    else
                    {
                        //获取到数据
                        std::cout << "recv from client: " << buf
                            << " clientfd: " << vec_pfds[i].fd << std::endl;
                    }
                }
            }
            else if (vec_pfds[i].revents & POLLERR)
            {
                //TODO,暂不处理
            }
        }//遍历结束括号（代码多时方便看）

        if (exist_invalid_fd)
        {
            //存在无效fd，统一清理一次
            for (auto iter = vec_pfds.begin(); iter != vec_pfds.end();)
            {
                if (iter->fd == INVALID_FD)
                    iter = vec_pfds.erase(iter);
                else
                    ++iter;//因为可能删除元素，所以不能在上面做++操作
            }
        }

    }//服务端主体循环结束

    //回收资源
    for(auto pfd : vec_pfds)
        close(pfd.fd);
}

int main(int argc, char* argv[])
{
    test_poll_server();

    return 0;
}
