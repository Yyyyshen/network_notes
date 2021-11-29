// cpp_server_dev.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//《C++服务器开发精髓》学习
//

//
//C++特性和惯用法
//

//
//RAII
// 
// 从一个基本的win上的简单服务开始
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
void
simple_win_tcp_server()
{
	WORD wVersionRequested = MAKEWORD(2, 2);	//高低8位创建一个2字节unsigned short，表示使用2.2版本的winsock
	WSADATA wsaData;							//存放wsa初始化后的数据

	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
		return;									//结果必须检查

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();							//检查版本，不符合则回收资源退出
		return;
	}

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
	if (sockSrv == -1)
	{
		WSACleanup();							//创建监听socket，失败则回收资源退出
		return;
	}

	SOCKADDR_IN addrSrv;						//socket通信地址信息
	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrSrv.sin_port = htons(6000);				//ntohs = net to host short int 16位
												//htons = host to net short int 16位
												//ntohl = net to host long int 32位
												//htonl = host to net long int 32位
	addrSrv.sin_family = AF_INET;

	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == -1)
	{											//绑定socket和地址信息并检查结果
		closesocket(sockSrv);
		WSACleanup();
		return;
	}

	if (listen(sockSrv, 1000) == -1)
	{											//开启监听，检查结果
		closesocket(sockSrv);
		WSACleanup();
		return;
	}

	SOCKADDR_IN addrClient;						//准备接收客户端连接
	int len = sizeof(SOCKADDR);
	char msg[] = "Hello,World!";

	while (true)								//无限循环，长期运行
	{
		SOCKET sockClient = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
		if (sockClient == -1)					//客户连接到来
			break;

		send(sockClient, msg, strlen(msg), 0);	//发送简单数据
		closesocket(sockClient);
	}

	closesocket(sockSrv);						//循环出错跳出，清理资源
	WSACleanup();
	return;
}
// 基本的代码，但太多重复的错误处理
// 可以使用goto（难维护）或do_while(0)优化
void
simple_win_tcp_server_2()
{
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
		return;

	SOCKET sockSrv = -1;

	do
	{

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
			break;								//错误处理都改为break出循环

		sockSrv = socket(AF_INET, SOCK_STREAM, 0);
		if (sockSrv == -1)
			break;

		SOCKADDR_IN addrSrv;
		addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		addrSrv.sin_port = htons(6000);
		addrSrv.sin_family = AF_INET;

		if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == -1)
			break;

		if (listen(sockSrv, 1000) == -1)
			break;

		SOCKADDR_IN addrClient;
		int len = sizeof(SOCKADDR);
		char msg[] = "Hello,World!";

		while (true)
		{
			SOCKET sockClient = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
			if (sockClient == -1)
				break;

			send(sockClient, msg, strlen(msg), 0);
			closesocket(sockClient);
		}

	} while (0);

	if (sockSrv != -1)							//不确定sock有没有成功创建，做一次判断
		closesocket(sockSrv);
	WSACleanup();								//统一做资源回收逻辑
	return;
}
// 其实这些都是C的写法，在C++中，可以利用RAII
class ServerSocket
{
public:
	using this_type = ServerSocket;				//定义所需类型别名，后期可灵活修改
	using bool_type = bool;
	using sock_type = SOCKET;
public:
	ServerSocket()								//构造，初始化成员（这里直接在成员声明时定义初始值）
	{

	}
	~ServerSocket()								//析构，自动回收资源
	{
		if (m_listen_sock != -1)
			::closesocket(m_listen_sock);
		if (m_init)
			WSACleanup();
	}
public:											//正常业务函数
	bool_type init()							//不能在构造中直接分配资源，失败的话很难处理
	{
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		int err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
			return false;

		m_init = true;

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
			return false;

		m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (m_listen_sock == -1)
			return false;

		return true;
	}

	bool_type do_bind(const char* ip, unsigned short port)
	{
		SOCKADDR_IN addrSrv;
		inet_pton(AF_INET, ip, &addrSrv.sin_addr);
		addrSrv.sin_port = htons(port);
		addrSrv.sin_family = AF_INET;
		if (::bind(m_listen_sock, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == -1)
			return false;
		return true;
	}

	bool_type do_listen(int backlog = 1024)
	{
		if (::listen(m_listen_sock, backlog) == -1)
			return false;
		return true;
	}

	bool_type do_accept()
	{
		SOCKADDR_IN addrClient;
		int len = sizeof(SOCKADDR);
		char msg[] = "Hello,World!";

		while (true)
		{
			SOCKET sockClient = ::accept(m_listen_sock, (SOCKADDR*)&addrClient, &len);
			if (sockClient == -1)
				break;
			::send(sockClient, msg, strlen(msg), 0);
			::closesocket(sockClient);
		}
		return false;
	}

private:
	bool_type m_init = false;
	sock_type m_listen_sock = -1;
};
void
simple_win_tcp_server_raii()
{
	ServerSocket server;

	do
	{
		if (!server.init())
			break;
		if (!server.do_bind("0.0.0.0", 6000))
			break;
		if (!server.do_listen())
			break;
		if (!server.do_accept())
			break;
	} while (0);

	return;
}
// 像智能指针、lockguard，都是这样的应用
//

//
//pimpl
// 当一个类的设计中，成员变量过多，暴露了太多细节
// 可以将这些成员都封装成一个impl类
// 在本类中用一个指向impl类的指针作为成员
class SocketClient
{
private:
	struct Impl;									//内部实现类，也可用struct
public:
	using this_type = SocketClient;
	using impl_type = Impl;
	using sptr_type = std::unique_ptr<impl_type>;
public:
	SocketClient()
		:m_pImpl(std::make_unique<Impl>())
	{}
	~SocketClient() 								//使用智能指针，不需要显式删除
	{}
private:
	sptr_type m_pImpl;
};
struct SocketClient::Impl
{
	SOCKET m_socket;
	//...很多成员
};
// 
//

//
//
//

int main()
{
	std::cout << "Hello World!\n";
	//simple_win_tcp_server();
	simple_win_tcp_server_raii();
}
