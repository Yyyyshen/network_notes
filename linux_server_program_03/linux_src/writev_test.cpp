//Copyright(c) by yyyyshen 2021

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5
#define BUFFER_SIZE 1024
#define FILE_NAME "./byteorder_test.cpp"

//http状态码定义
static const char* status_line[3] = {
    "200 OK",
    "400 Not found",
    "500 Internal server error"
};

void
test_writev()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    struct sockaddr_in cli_addr;
    socklen_t cli_addrlen = sizeof(cli_addr);
    int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
    if (clifd < 0)
        printf("accept failed, errno: %d\n", errno);
    else
    {
        //保存http状态、头和空行
        char head_buf[BUFFER_SIZE];
        memset(head_buf, '\0', BUFFER_SIZE);
        //存放响应文件内容
        char* file_buf;
        struct stat file_stat;//文件属性
        bool valid = true;//文件是否有效标志
        int len = 0;//记录head_buf使用长度
        if(stat(FILE_NAME, &file_stat) < 0) //目标文件不存在
            valid = false;
        else
        {
            //是否是目录
            if (S_ISDIR(file_stat.st_mode))
                valid = false;
            else if (file_stat.st_mode & S_IROTH) //有读取权限
            {
                //读入
                int fd = open(FILE_NAME, O_RDONLY);
                file_buf = new char[file_stat.st_size + 1];
                memset(file_buf, '\0', file_stat.st_size +1);
                if (read(fd, file_buf, file_stat.st_size) < 0)
                    valid = false;
            }
            else
                valid = false;
        }

        if (valid)
        {
            //文件有效，回复内容
            //头部拼接
            ret = snprintf(head_buf, BUFFER_SIZE-1, "%s %s\r\n",
                    "HTTP/1.1", status_line[0]);
            len += ret;
            ret = snprintf(head_buf + len, BUFFER_SIZE-1-len,
                    "Content-Length: %d\r\n", file_stat.st_size);
            len += ret;
            ret = snprintf(head_buf+len,BUFFER_SIZE-1-len,"%s","\r\n");
            //将头和文件内容加入iovec集中发送
            struct iovec iv[2];
            iv[0].iov_base = head_buf;
            iv[0].iov_len = strlen(head_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(clifd, iv, 2);
        }
        else
        {
            //目标文件无效，报内部错误
            ret = snprintf(head_buf, BUFFER_SIZE-1, "%s %s\r\n",
                    "HTTP/1.1", status_line[2]);
            len += ret;
            ret = snprintf(head_buf + len, BUFFER_SIZE-1-len, "%s","\r\n");
            send(clifd,head_buf,strlen(head_buf),0);
        }

        close(clifd);
        delete[] file_buf;
        file_buf = nullptr;
    }

    close(listenfd);
}

int main(int argc, char* argv[])
{
    test_writev();

    return 0;
}
