//Copyright(c) by yyyyshen 2021

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 3000
#define SEND_DATA "helloworld"

bool
send_data(int fd, const char* buf, int len)
{
	//记录已发送字节数
	int sent_bytes = 0;

	while (true)
	{
		int ret = send(fd, buf + sent_bytes, len - sent_bytes, 0);
		if (ret == -1)
		{
			if (errno == EWOULDBLOCK)
			{
				break;//窗口满了，应当缓存没发出去的数据，等待一会尝试再次发送
			}
			else if (errno == EINTR)
				continue;
			else
				break;
		}
		else if (ret == 0)
			return false;

		sent_bytes += ret;
		if (sent_bytes == len)
			break;//完整发完，结束
	}
}

void
test_client()
{
    //创建连接socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cout << "client socket create failed" << std::endl;
        return;
    }

    //连接服务器
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    serveraddr.sin_port = htons(SERVER_PORT);
    if (connect(clientfd, (sockaddr*)&serveraddr, sizeof(serveraddr)) == -1)
    {
        std::cout << "connet socket failed" << std::endl;
        return;
    }

    //发送数据
    int ret = send(clientfd, SEND_DATA, strlen(SEND_DATA), 0);
    if(ret != strlen(SEND_DATA))
    {
        std::cout << "send data error" << std::endl;
        return;
    }

    std::cout << "send data to server successfully, data: " << SEND_DATA << std::endl;

    //接收数据
    char recvBuf[1024] = {0};
    ret = recv(clientfd, recvBuf, 1024, 0);
    if (ret > 0)
        std::cout << "recv data successfully, data: " << recvBuf << std::endl;
    else
        std::cout << "recv data error, data: " << recvBuf << std::endl;

    //关闭socket
    close(clientfd);
}

int main(int argc,char* argv[])
{
    test_client();

    return 0;
}
