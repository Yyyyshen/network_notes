//Copyright(c) by yyyyshen 2021
//

#include "chat_use_poll_public.h"

void
chat_client()
{
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &remote_addr.sin_addr);
    remote_addr.sin_port = htons(SERVER_PORT);

    int connfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(connfd >= 0);
    if (connect(connfd, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0)
    {
        printf("conn failed\n");
        close(connfd);
        return;
    }

    //poll管理连接和标准输入
    pollfd fds[0];
    fds[0].fd = 0;//标准输入
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = connfd;//连接socket
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];//管道用于重定向
    int ret = pipe(pipefd);
    assert(ret != -1);

    while(true)
    {
        ret = poll(fds, 2, -1);
        if (ret < 0)
            break;//poll失败

        //服务端事件
        if (fds[1].revents & POLLRDHUP)
            break;//被服务端断开
        else if (fds[1].revents & POLLIN)
        {
            //接收消息，拷贝到应用层再标准输出
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            printf("%s\n", read_buf);
        }

        //标准输入事件
        if (fds[0].revents & POLLIN)
        {
            //将用户输入的数据从标准输入重定向到可写管道
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            //将可读管道数据重定向到网络连接，实现零拷贝
            ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }

    close(connfd);
}

int main(int argc, char* argv[])
{
    chat_client();

    return 0;
}
