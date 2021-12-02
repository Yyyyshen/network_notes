//Copyright(c) by yyyyshen 2021

/*

本例流程

						   创建监听socket
								 ↓
							绑定地址端口
								 ↓
							  开始监听
								 ↓
		  |-----→将socket加入fd_set，监听可读事件←----------|
		  |    无↑              ↓                           |
		  |       -------指定时间是否有事件                   |
		  |                      ↓有                         |
		  |         可读事件是否时发生于监听socket            |
		  |     是   ↙                    否   ↘            |
		  接收连接，产生新socket        从client socket上接收数据


注意事项：
	select函数调用前后可能会修改三个set中的内容，所以每次select时，都是FD_ZERO清零后重新遍历添加
	超时时间同理（根据linux系统表现修改，但Windows下不修改）
	超时时间设置为0则是单纯检查是否有事件，不进行超时等待，立即返回
	超时时间设置为NULL则是一直阻塞，直到事件触发
	第一个参数设置为所有fd中最大值+1，所以每产生一个socket都检查一下，更新这个最大值（Windows下不使用这个参数，但为了兼容，保留了同样的参数格式）

select缺陷：
	每次调用，都需要清零重新添加，并且有事件之后也不明确是哪个连接，需要再次遍历确认；连接过多时开销很大
	linux下单进程监视文件描述符数量最大限制为1024，可以通过修改宏定义重新编译内核调整限制，但麻烦且效率并不高
	linux中select函数底层实现使用了poll

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <errno.h>

//无效fd
#define INVALID_FD -1
//服务端口
#define SERVER_PORT 3000

void
test_select_server()
{
	//创建监听socket
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == INVALID_FD)
	{
		std::cout << "listenfd create failed" << std::endl;
		return;
	}

	//初始化服务器地址二元组
	sockaddr_in bindaddr;
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindaddr.sin_port = htons(SERVER_PORT);
	//绑定地址
	if (bind(listenfd, (sockaddr*)&bindaddr, sizeof(bindaddr)) == -1)
	{
		std::cout << "bind listenfd failed" << std::endl;
		return;
	}

	//开始监听
	if (listen(listenfd, SOMAXCONN) == -1)
	{
		std::cout << "start listen failed" << std::endl;
		return;
	}

	//存储来自客户端的连接socket
	std::vector<int> vec_cfds;
	//计算最大fd值，select第一个参数需要
	int maxfd;

	while (true)
	{
		//初始化set
		fd_set readset;
		FD_ZERO(&readset);

		//将监听socket加入检测
		FD_SET(listenfd, &readset);
		maxfd = listenfd;

		//将连进来的客户端连接加入带检测事件列表
		int veclen = vec_cfds.size();
		for (int i = 0; i < veclen; ++i)
		{
			if (vec_cfds[i] != -1)
			{
				FD_SET(vec_cfds[i], &readset);

				//确认最大值
				if (maxfd < vec_cfds[i])
					maxfd = vec_cfds[i];
			}
		}

		//超时时间
		timeval tm;
		tm.tv_sec = 1;
		tm.tv_usec = 0;
		//暂时只监听可读事件
		int ret = select(maxfd + 1, &readset, NULL, NULL, &tm);
		if (ret == -1)
		{
			//出错退出
			if (errno != EINTR)
				break;
		}
		else if (ret == 0)
		{
			//超时继续尝试
			continue;
		}
		else
		{
			//检测到事件了
			if (FD_ISSET(listenfd, &readset))
			{
				//监听socket有事件，说明是有新连接进来
				sockaddr_in clientaddr;
				socklen_t clientaddrlen = sizeof(clientaddr);

				//接收连接
				int clientfd = accept(listenfd, (sockaddr*)&clientaddr, &clientaddrlen);
				if (clientfd == INVALID_FD)
					break;

				//接收连接后先不接收数据，只是存起来
				std::cout << "accept a client connection, fd: " << clientfd << std::endl;
				vec_cfds.push_back(clientfd);
			}
			else
			{
				//其他连接有读事件，说明有数据发进来
				char recvbuf[1024];
				int veclen = vec_cfds.size();
				//遍历查看哪个连接产生的事件
				for (int i = 0; i < veclen; ++i)
				{
					//检查socket状态以及事件状态
					if (vec_cfds[i] != INVALID_FD && FD_ISSET(vec_cfds[i], &readset))
					{
						memset(recvbuf, 0, sizeof(recvbuf));
						//非监听socket，接收数据
						int length = recv(vec_cfds[i], recvbuf, 1024, 0);
						//检查数据长度
						if (length <= 0)
						{
							std::cout << "recv data error, clientfd: "
								<< vec_cfds[i] << std::endl;
							//发生错误，关闭套接字
							close(vec_cfds[i]);
							//不删除元素，而是置为无效
							vec_cfds[i] = INVALID_FD;
							continue;
						}

						std::cout << "recv data: " << recvbuf
							<< "clientfd: " << vec_cfds[i] << std::endl;
					}
				}
			}
		}

	}

	//发生错误退出，进行资源回收
	for (auto f : vec_cfds)
		if (f != INVALID_FD)
			close(f);

	close(listenfd);
}

int main(int argc, char* argv[])
{

	test_select_server();

	return 0;
}
