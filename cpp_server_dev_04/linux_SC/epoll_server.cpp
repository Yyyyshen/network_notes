//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <poll.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <errno.h>

void
test_epoll_server()
{
    //创建监听socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        std::cout << "listenfd create fail" << std::endl;
        return;
    }

    //设置非阻塞
    int oldf = fcntl(listenfd, F_GETFL, 0);
    int newf = oldf | O_NONBLOCK;
    if (fcntl(listenfd, F_SETFL, newf) == -1)
    {
        std::cout << "listenfd set nonblock fail" << std::endl;
        close(listenfd);
        return;
    }

    //设置IP和端口重用
    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char*)&on, sizeof(on));

    //初始化绑定地址
    sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(3000);
    //绑定
    if (bind(listenfd, (sockaddr*)&bindaddr, sizeof(bindaddr)) == -1)
    {
        std::cout << "listenfd bind addr fail" << std::endl;
        close(listenfd);
        return;
    }

    //监听
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        std::cout << "listen error" << std::endl;
        close(listenfd);
        return;
    }

    //开始使用epoll管理
    int epollfd = epoll_create(1);
    if(epollfd == -1)
    {
        std::cout << "epollfd create fail" << std::endl;
        close(listenfd);
        return;
    }

    //添加需要监听的事件
    epoll_event ev_listenfd;
    ev_listenfd.data.fd = listenfd;
    ev_listenfd.events = EPOLLIN;
    //模式设置，注释则为使用默认LT模式
    //ev_listenfd.events |= EPOLLET;

    //将listenfd的事件监听添加到epollfd
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev_listenfd) == -1)
    {
        std::cout << "epollfd add listenfd fail" << std::endl;
        close(listenfd);
        return;
    }

    //开始事件处理
    int n;
    while (true)
    {
        //创建一个事件数组，接收wait返回结果
        epoll_event epoll_events[1024];
        n = epoll_wait(epollfd, epoll_events, 1024, 1000);
        
        //根据事件数量处理
        if (n < 0)
        {
            //中断或出错
            if (errno == EINTR)
                continue;
            break;
        }

        if (n == 0)
            continue;//超时

        //有事件产生
        for (size_t i = 0; i < n; ++i)
        {
            if (epoll_events[i].events & EPOLLIN)
            {
                //可读事件
                if (epoll_events[i].data.fd == listenfd)
                {
                    //新连接进来
                    sockaddr_in clientaddr;
                    socklen_t clientaddrlen = sizeof(clientaddr);
                    int clientfd = accept(listenfd, (sockaddr*)&clientaddr, &clientaddrlen);

                    if(clientfd == -1)
                    {
                        std::cout << "clientfd create fail" << std::endl;
                        continue;
                    }

                    //设置非阻塞
                    int oldf = fcntl(clientfd, F_GETFL, 0);
                    int newf = oldf | O_NONBLOCK;
                    if (fcntl(clientfd, F_SETFL, newf) == -1)
                    {
                        close(clientfd);
                        continue;
                    }

                    //将新连接fd添加到epoll事件列表
                    epoll_event ev_clientfd;
                    ev_clientfd.data.fd = clientfd;
                    ev_clientfd.events = EPOLLIN | EPOLLOUT;
                    //ev_clientfd.events |= EPOLLET;
                    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev_clientfd) == -1)
                    {
                        std::cout << "epollfd add clientfd fail" << std::endl;
                        close(clientfd);
                        continue;//书中例子用了太多else，可读性不太好，自己改一下
                    }

                    std::cout << "accept new client, fd: " << clientfd << std::endl;
                }
                else
                {
                    //clientfd收到数据
                    char ch;
                    //测试触发模式，一次只处理1字节
                    int ret = recv(epoll_events[i].data.fd, &ch, 1, 0);
                    if (ret < 0)
                    {
                        if (errno != EWOULDBLOCK && errno != EINTR)
                        {
                            //的确是出错，则删掉检测列表中的项
                            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, 
                                        epoll_events[i].data.fd, NULL) != -1)
                            {
                                std::cout << "client disconnected, clientfd: "
                                    << epoll_events[i].data.fd << std::endl;
                            }
                            close(epoll_events[i].data.fd);
                        }
                        //其他情况，重试
                        continue;
                    }

                    if (ret == 0)
                    {
                        //对端关闭连接，和出错一样的处理
                        if (epoll_ctl(epollfd, EPOLL_CTL_DEL,
                                    epoll_events[i].data.fd, NULL) != -1)
                        {
                            std::cout << "clinet disconnected, clientfd: "
                                << epoll_events[i].data.fd << std::endl;
                        }
                        close(epoll_events[i].data.fd);
                        continue;
                    }

                    //成功收到信息
                    std::cout << "recv from client: " << ch 
                        << " clientfd: " << epoll_events[i].data.fd << std::endl;
                    //如果是默认LT模式，一次输出一个字节，一个包分开打印
                    //若为ET模式，一次输出一个字节，只有收到下一个包才打印下一个字节
                    //
                }
            }
            else if (epoll_events[i].events & EPOLLOUT)
            {
                //可写事件
                //若为默认LT模式，会一直输出；ET模式会触发一次
                //所以，不管是哪种模式，如果不需要写，应当随时动态移除写事件检测
                std::cout << "clientfd: " << epoll_events[i].data.fd
                    << " EPOLLOUT triggered" << std::endl;

                //若本次写完暂不需要继续写，则应修改检测，去掉写事件监听
                //当然，如果是ET模式，数据需要分片发送时，还是需要继续注册写事件监听
                //直到完全把数据发送完整
                epoll_event ev_clientfd;
                ev_clientfd.data.fd = epoll_events[i].data.fd;
                ev_clientfd.events = EPOLLIN | EPOLLET;//不再关心OUT可写事件
                if (epoll_ctl(epollfd, EPOLL_CTL_MOD, 
                            epoll_events[i].data.fd, &ev_clientfd) != -1)
                    std::cout << "modify event succ" << std::endl;
            }
            else if (epoll_events[i].events & EPOLLERR)
            {
                //异常事件，暂不处理
            }
        }//事件处理循环结束
    }//主服务循环结束

    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_epoll_server();

    return 0;
}
