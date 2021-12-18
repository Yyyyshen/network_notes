//Copyright(c) by yyyyshen 2021
//

#include "chat_use_poll_public.h"

struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

void
chat_server()
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    //创建user数组，分配一定数量的client_data结构对象
    //每个socket连接获得一个这样的对象，并且是依据socket值直接索引其位置
    //占用空间大一些，但效率会提高
    client_data* users = new client_data[FD_LIMIT];
    //限制用户数量，虽然user数组很大，但那是为了增加索引效率
    pollfd fds[USER_LIMIT + 1];//+1 放监听fd
    int user_counter = 0;
    //初始化事件对象
    for(int i = 0; i <= USER_LIMIT; ++i)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    //添加监听fd管理
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while(true)
    {
        ret = poll(fds, user_counter+1, -1);
        if (ret < 0)
            break;

        for (int i = 0; i < user_counter + 1; ++i)
        {
            //如果是监听socket事件
            if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN))
            {
                //接收连接
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, & cli_addrlen);
                if (clifd < 0)
                    continue;//接收出错，跳过

                //判断请求是否超过限制
                if (user_counter >= USER_LIMIT)
                {
                    //回复服务器忙，并关闭连接，不纳入管理
                    const char* info = "too many users\n";
                    send(clifd, info, strlen(info), 0);
                    close(clifd);
                }

                //给新连接分配资源，纳入管理
                ++user_counter;
                users[clifd].address = cli_addr;
                setnonblocking(clifd);
                fds[user_counter].fd = clifd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf("new user conn, now have %d users\n", user_counter);
            }
            else if (fds[i].revents & POLLRDHUP)
            {
                //客户端断开连接
                //服务器关闭对应连接，计数-1
                users[fds[i].fd] = users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];//把最后一个填充过来，空出位置
                --user_counter;
                printf("a client left\n");

                //当前位置已用列表最后一个换过来，还需要继续遍历这个索引
                --i;
            }
            else if (fds[i].revents & POLLIN)
            {
                //一个客户端发了消息
                int clifd = fds[i].fd;
                memset(users[clifd].buf, '\0', BUFFER_SIZE);
                ret = recv(clifd, users[clifd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d\n",
                        ret, users[clifd].buf, clifd);
                if (ret < 0)
                {
                    //读错误，关闭连接
                    close(clifd);
                    users[fds[i].fd] = users[fds[user_counter].fd];
                    fds[i] = fds[user_counter];
                    --user_counter;
                    --i;
                }
                else if (ret == 0)
                {

                }
                else
                {
                    //正确接收消息，通知其他socket准备写数据
                    for(int j = 1; j <= user_counter; ++j)
                    {
                        if (fds[j].fd == clifd)
                            continue;//不向发送该消息的客户端推送
                        fds[j].events != ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[clifd].buf;
                    }
                }
            }
            else if (fds[i].revents & POLLOUT)
            {
                //向每个已连接客户端推送消息
                int clifd = fds[i].fd;
                if(!users[clifd].write_buf)
                    continue;
                ret = send(clifd, users[clifd].write_buf, strlen(users[clifd].write_buf), 0);
                users[clifd].write_buf = NULL;//发完清空缓冲区
                //重新注册回可读事件
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }

    //资源回收
    delete[] users;
    close(listenfd);
}

int main(int argc, char* argv[])
{
    chat_server();

    return 0;
}
