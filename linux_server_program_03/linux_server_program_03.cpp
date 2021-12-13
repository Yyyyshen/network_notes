// linux_server_program_03.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//深入解析高性能服务器编程
//

//
//Linux网络编程基础API
//

//
//socket地址API
// 
//主机字节序和网络字节序
// 32位机器CPU，累加器一次装载4字节，数据在内存中的顺序影响装载后的值
// 分为大端和小端
// 大端是高位在低地址，低位在高地址；小端是反过来
void
test_byteorder()
{
	union test
	{
		short value;
		char union_bytes[sizeof(short)];
	};
	test u;
	u.value = 0x0102;
	if (u.union_bytes[0] == 1 && u.union_bytes[1] == 2)
		std::cout << "big endian" << std::endl;
	else if (u.union_bytes[1] == 1 && u.union_bytes[0] == 2)
		std::cout << "little endian" << std::endl;
	else
		std::cout << "unknown..." << std::endl;
}
// 现代PC大多是小端字节序，因此又称为主机字节序
// 两台不同字节序的主机直接传递数据会出错
// 方法是，发送端总是把数据转成大端，接收端总是默认传过来的是大端，然后根据自身字节序选则是否转换
// 所以大端又称为网络字节序
// 同一台机器上两个进程通信，也要考虑字节序问题（例如C和JAVA，java采用大端）
// htonl、htons、ntohl、ntohs
// h表示host、n表示network、l和s表示long和short，长整型常用来转换IP，短整型转端口
// 
//通用socket地址
// Linux下最基本socket地址
// struct sockaddr
// {
//		sa_family_t sa_family;
//		char sa_data[14];
// }
// sa_family是地址族变量，对应协议族变量
// AF_UNIX		PF_UNIX		Unix本地域协议族	文件路径名，长度可达108字节
// AF_INET		PF_INET		TCP/IPv4协议族		16bit端口和32bitIPv4地址，共6字节
// AF_INET6		PF_INET6	TCP/IPv6协议族		16bit端口和32bit流标识，128bitIPv6地址，32bit范围ID，共26字节
// 可见，sockaddr只能用于IPv4
// 新的通用地址结构体
// struct sockaddr_storage
// {
//		sa_family_t sa_family;
//		unsigned long int __ss_align;
//		char __ss_padding[128-sizeof(__ss_align)];
// }
// 提供了足够大的空间用于存储地址值，并且是内存对齐(__ss_align)
// 
//专用地址结构
// 通用的地址结构只有数据的数组成员，需要自己把IP端口号做处理才能设置或获取
// Linux提供专用于各个协议族的地址结构体
// sockaddr_un、sockaddr_in、sockaddr_in6
// 最常用的v4结构如下
// struct sockaddr_in
// {
//		sa_family_t sin_family;
//		u_int16_t sin_port;
//		struct in_addr sin_addr;
// }
// 其中
// struct in_addr
// {
//		u_int32_t s_addr;
// }
// 专用的socket地址更方便设置或获取成员值，实际使用时，设置后直接强转为sockaddr类型即可，因为接口参数都是这个类型
// 
//IP地址转换函数
// 人习惯用点分制字符串，而地址结构中是整型，所以需要转换
// inet_addr、inet_aton、inte_ntoa
// a表示address、n表示network，相互转换
// 也可以使用新函数兼容IPv4和v6
// inet_pton、inet_ntop
// 多一个参数传递地址族，还有地址族长度大小，涉及到两个辅助宏
// INET_ADDRSTRLEN 16
// INET_ADDRESTRLEN 46
//

//
//创建socket
// UNIX/Linux哲学 万物皆文件
// socket也是一个可读可写，可控制可关闭的文件描述符(fd)
// 使用socket系统函数可创建
// int fd = socket(int domain,int type,int protocol);
// 参数domain表示用哪个协议族，type为服务类型（流和数据报），protocol通常都设置为0
// 返回值为socket fd，失败返回-1
// 
//绑定socket
// int bind(int sockfd,const struct sockaddr* addr,socklen_t addrlen);
// 将创建的socket文件描述符和一个socket地址结构绑定
// 成功返回0，失败返回-1并设置errno（EACCES，无权限；EADDRINUSE，被占用）
// 
//监听socket
// int listen(int sockfd,int backlog);
// 绑定socket后需要创建监听队列存放待处理客户连接
// 参数backlog表示内核监听队列的最大长度
// 如果超过backlog，服务器将不接受新的客户端连接
// 
//服务器例
// linux_src/backlog_test.cpp
// 例中backlog设置了5，测试连接
// 处于ESTABLISHED状态的连接只有6个（backlog值+1）
// 其他连接处于SYN_RCVD状态
// 
//接受连接
// int accept(int sockfd, struct sockaddr* addr, socklen_t addrlen);
// 例
// linux_src/accept_test.cpp
// 
//发起连接
// int connect(int sockfd,const struct sockaddr* server_addr, socklen_t addrlen);
// 客户端通过该系统调用发起连接
// 
//关闭连接
// int close(int fd);
// fd参数是待关闭的socket，close并不总是立即关闭一个连接，而是将fd引用计数-1，只有fd引用计数为0才真正关闭
// 要立刻关闭（非引用计数-1）可以使用shutdown
// int shutdown(int sockfd, int howto);
// 参数howto行为可选SHUT_RD/SHUT_WR/SHUTRDWR
// 
//数据读写
// TCP
// ssize_t recv(int sockfd, void *buf, size_t len, int flags);
// ssize_t send(int sockfd, const void *buf, size_t len, int flags);
// 其中flags选项一般是0
// 例
// linux_src/send_test.cpp
//

int main()
{
	test_byteorder();
}
