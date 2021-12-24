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
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "process_pool.hpp"

//处理客户连接，作为进程池的模板参数
class cgi_conn final
{
public:
    cgi_conn() = default;
    ~cgi_conn() = default;

public:
    //需按照池中模板参数指定的函数格式来实现
    void init(int epollfd, int sockfd, const sockaddr_in& client_addr)
    {
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }

    //业务处理函数
    void process()
    {
        int idx = 0;
        int ret = -1;

        //处理收到的数据
        while (true)
        {
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - idx - 1, 0);
            if (ret < 0)
            {
                if (errno != EAGAIN)
                    //出错退出
                    removefd(m_epollfd, m_sockfd);
                //如果是EAGAIN，暂无数据可读
                break;
            }
            else if (ret == 0)
            {
                //对端关闭连接，本端也关闭
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else
            {
                m_read_idx += ret;//记录已读长度
                //在已读数据中检测\r\n模拟一条消息结束
                for (; idx < m_read_idx; ++idx)
                    if ((idx >= 1) && (m_buf[idx-1] == '\r') && (m_buf[idx] == '\n'))
                        break;
                //没找到，则继续读
                if (idx == m_read_idx)
                    continue;

                //找到了则处理cgi逻辑
                m_buf[idx - 1] = '\0';//字符串结束标记
                char* file_name = m_buf;
                //判断cgi程序是否存在
                if (access(file_name, F_OK) == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                //有则创建子进程执行cgi
                ret = fork();
                if (ret == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else if (ret > 0)
                {
                    //本进程只需关闭连接
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else
                {
                    //负责处理cgi的子进程将便准输出定向到socket
                    close(STDOUT_FILENO);//关闭标准输出
                    dup(m_sockfd);//开启一个值最小的fd，此时正好为标准输出，实现了重定向
                    execl(m_buf, m_buf, 0);//执行cgi程序
                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;//处理读缓冲区，标记已读位置
};

int cgi_conn::m_epollfd = -1;

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 50000

int main(int argc, char* argv[])
{
    //负责创建和回收监听socket
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

    //创建并启动进程池
    process_pool<cgi_conn>* pool = process_pool<cgi_conn>::create(listenfd);
    if (pool)
    {
        //进入主循环
        pool->run();
        delete pool;
    }
    //回收
    close(listenfd);
    return 0;
}
