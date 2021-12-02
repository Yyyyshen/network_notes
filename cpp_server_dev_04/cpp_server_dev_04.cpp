// cpp_server_dev_04.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <WinSock2.h>

//
//网络编程重点
//

//
//socket
// bind
// listen
// connect
// accept
// send/recv
// select           判断一组socket上的读写和异常
// gethostbyname    通过域名获取地址
// close
// shutdown
// setsockopt/getsockopt
// 
// linux下可以使用man命令查看手册
//

//
//TCP通信基本流程
// 服务端
//	socket函数创建监听fd（linux下万物皆文件
//	bind函数将监听socket绑定到某IP和端口二元组
//	listen函数开启监听
//	accept函数阻塞等待
//	接收客户端的连接，产生一个新的连接socket（区分监听socket
//	基于新socket调用send、recv进行数据沟通
//	通信结束，close函数关闭监听socket
// 客户端
//	socket创建连接socket
//	connect函数尝试连接服务器
//	连接成功后调用send、recv进行数据通信
// 通信结束，close函数关闭连接socket
//

//
//基本实现
// linux下的server和client
//	放在项目其他文件下
// 
// C设计跨平台网络通信库一些定义
#ifdef _WIN32
using socket_type = SOCKET;			//win下必须先用WSAStartup函数显式加载socket函数相关dll，不像linux可以直接使用
#define GetSocketError() WSAGetLastError()
#define SOCKWOULDBLOCK WSAEWOULDBLOCK
#else								//将linux下一些操作向windows对齐
using socket_type = int;
#define INVALID_HANDLE_VALUE (-1)	//对应windows下创建socket失败时为返回值-1定义的宏
#define closesocket(s) close(s)		//对应windows下关闭socket，统一写法为closesocket（win下有同名close函数但不能用于关闭socket）
#define GetSocketError() errno		//统一定义获取错误码方式
#define SOCKWOULDBLOCK WEWOULDBLOCK	//套接字操作函数不能立即完成时，两平台错误码定义不同，统一一下
#endif
// 
// 还有I/O复用技术
//	windows特有 WSAPoll和IOCP
//	linux特有 poll、epoll
//	都支持的 select
/*

elect、poll、epoll的区别

				select					poll					epoll
底层数据结构	数组					链表					红黑树和双链表
获取就绪的fd	遍历					遍历					事件回调
事件复杂度		O(n)					O(n)					O(1)
最大连接数		1024					无限制					无限制
fd数据拷贝		每次调用select，需要	每次调用poll，需要		使用内存映射(mmap)，不需要
				将fd数据从用户空间拷	将fd数据从用户空间		从用户空间频繁拷贝fd数据到内核空间
				贝到内核空间			拷贝到内核空间

但epoll_wait依旧是阻塞的，产生信号驱动模型

信号驱动IO不再用主动询问的方式去确认数据是否就绪，
而是向内核发送一个信号（调用sigaction的时候建立一个SIGIO的信号），
然后应用用户进程可以去做别的事，不用阻塞。
当内核数据准备好后，再通过SIGIO信号通知应用进程，数据准备好后的可读状态。
应用用户进程收到信号之后，立即调用recvfrom，去读取数据。

信号驱动IO模型，在应用进程发出信号后，是立即返回的，不会阻塞进程。
它已经有异步操作的感觉了。但是，数据复制到应用缓冲的时候，应用进程还是阻塞的。
回过头来看下，不管是BIO（阻塞），还是NIO（非阻塞），还是信号驱动，在数据从内核复制到应用缓冲的时候，都是阻塞的。

优化方案呢 AIO（真正的异步IO）
AIO实现了IO全流程的非阻塞，
就是应用进程发出系统调用后，是立即返回的，
但是立即返回的不是处理结果，而是表示提交成功类似状态的意思。
等内核数据准备好，将数据拷贝到用户进程缓冲区，发送信号通知用户进程IO操作执行完毕。

IO模型
阻塞I/O模型			同步阻塞
非阻塞I/O模型		同步非阻塞
I/O多路复用模型		同步阻塞
信号驱动I/O模型		同步非阻塞
异步IO（AIO）模型	异步非阻塞

同步阻塞(blocking-IO)简称BIO
同步非阻塞(non-blocking-IO)简称NIO
异步非阻塞(asynchronous-non-blocking-IO)简称AIO

一个经典生活的例子：
小明去吃，就这样在那里排队，等了一小时，然后才开始吃。(BIO)
小红也去，她一看要等挺久的，于是去逛会商场，每次逛一下，就跑回来看看，是不是轮到她了。于是最后她既购了物，又吃上了。（NIO）
小华一样，去吃，由于他是高级会员，所以店长说，你去商场随便逛会吧，等下有位置，我立马打电话给你。
于是小华不用坐着等，也不用每过一会儿就跑回来看有没有等到，最后也吃上了（AIO）

https://mp.weixin.qq.com/s/Xe3LMwsEaoFyFLJPcCCtcg

*/
// 
//

//
//bind函数
// 
//

int main()
{
	std::cout << "Hello World!\n";
}
