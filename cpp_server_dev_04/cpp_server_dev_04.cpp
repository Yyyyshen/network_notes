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
				（关于select的1024限制 https://blog.csdn.net/dog250/article/details/105896693）

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
// 绑定地址，使用了INADDR_ANY，如果不关心绑定ip，底层协议栈服务会自动选择一个合适ip，相当于0.0.0.0
// 只想在本地访问，则可以绑定回环地址127.0.0.1，局域网访问可以绑定192.168.x.x等
// 
// 绑定端口，一般指定固定端口
// 客户端的端口一般是连接发起时由操作系统随机分配（0~65535），相当于是默认bind了0作为端口；客户端也可以bind，但经常需要启动多个，绑同一个端口会冲突
// 服务端如果在bind中设置端口为0，也是随机分配一个监听端口，只不过一般不这么做
// 
// 常用命令
// lsof -i -Pn							查看连接
// nc -v [-p 12345] 127.0.0.1 3000		类似telnet，模拟客户端连接
// 
//

//
//select函数
// linux环境下
//	用于检测一组socket中是否有事件就绪，事件有三种
//	读事件就绪
//	 socket内核中，接收缓冲区字节数大于或等于低水位标记SO_RCVLOWAT；此时调用recv或read进行读操作，无阻塞，返回值大于0
//	 TCP连接的对端关闭；此时进行读操作，返回0
//	 在监听socket上有新的连接请求
//	 在socket上有未处理的错误
//	写事件就绪
//	 socket内核中，发送缓冲区中可用字节数（空闲位置大小）大于或等于低水位标记SO_SNDLOWAT；可以无阻塞写，返回值大于0
//	 写操作被关闭（close或shutdown）时，进行写操作会出发SIGPIPE信号
//   使用非阻塞connect连接成功或失败时
//	异常事件就绪
//	 socket收到带外数据（MSG_OOB）
// 
// 函数签名
// int select(int nfds,				//socket在linux下是fd，此参数值要设置为所有使用select检测事件的fd中值最大的一个+1
//		fd_set* readfds,			//需要监听可读事件的fd集合
//		fd_set* writefds,			//需要监听可写事件的fd集合
//		fd_set* exceptfds,			//需要监听异常事件的fd集合
//		struct timeval* timeout);	//超时事件，在指定时间内检查这些fd事件，若超过，select则返回
// 
// fd_set定义
/*

typedef long int __fd_mask;
#define __NFDBITS (8 * (int) sizeof (__fd_mask))
__FD_SETSIZE 之前说过select最大限制为1024
struct fd_set {
	//其实就是一个long int数组，数组大小16，每个元素8字节，每字节8bit，所以共存放8*8*16个fd；0表示无，1表示有
	__fd_mask __fds_bits[__FD_SETSIZE / __NFDBITS];
}

*/
// set操作
/*

将fd放入set
void FD_SET(int fd, fd_set* set);	//是个宏定义__FD_SET
具体实现 /usr/include/bits/select.h
#define __FD_SET(d, set) \
	((void) (__FDS_BITS (set)[__FD_ELT(d)] |= __FD_MASK(d)))

确认某fd标志位在数组中哪个下标位置
#define __FD_ELT(d) ((d) / __NFDBITS)
计算fd在对应bit位上的值，左移余数位
#define __FD_MASK(d) ((__fd_mask) 1 << ((d) % __NFDBITS))
最后是 |= 操作，将值设置到对应bit上
linux下使用的是bitmap位图法
windows下设置fd_set是从数组第0位置开始递增

在set中删除fd
void FD_CLR(int fd, fd_set* set);

清除set
void FD_ZERO(fd_set* set);
没有使用memset的原因是不想引入其他函数，并且数组长度并不大
#define __FD_ZERO(set) \
	do {
		unsigned int __i;
		fd_set* __arr = (set);
		for(__i = 0; __i < sizeof(fd_set) / sizeof(__fd_mask); ++__i)
			__FDS_BITS (_arr)[__i] = 0;
	} while(0) //经典do{} while(0)用法

当select函数返回时，使用FD_ISSET宏检查对应bit是否置位

*/
// 
// 示例在linux_SC下
//

//
//socket阻塞和非阻塞模式
// 对于connect、accept、send和recv（send讨论适用于linux中的write，recv讨论使用于read）
// 几种socket函数在两种模式下会有不同的表现
// 
// socket的创建
//	win和lin中，默认创建的socket都是阻塞模式
//	在创建时添加flag可设置为非阻塞模式 socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, PROTO);
//	通过accept接收连接返回的socket是阻塞的
//	可以使用accept4(listenfd, addr, addrlen, flags);将最后一个参数设置为SOCK_NONBLOCK，直接返回非阻塞socket
//	创建好的阻塞socket可以通过fcntl或ioctl函数设置为非阻塞模式
//		int oldf = fcntl(fd, F_GETFL, 0);
//		int newf = oldf | O_NONBLOCK;
//		fcntl(fd, F_SETFL, newf);
// 
// send/recv函数
//	send本质并不是向网络发送数据，而是将数据从应用发送缓冲区拷贝到内核缓冲区
//	具体什么时候从网卡缓冲区发送由TCP/IP协议栈决定
//	若设置socket选项 TCP_NODELAY （禁用nagel算法），则会立刻将存放到内核缓冲区的数据发出
//	若不设置，一般表现是，如果数据包很小，会积攒一会，凑成大一些的包才会发出
//	recv同理，是将内核缓冲区数据拷贝到应用缓冲区，拷贝完成后，内核中该部分会删除
//	
//	如果AB建立连接，A不断向B发送数据，B不调用recv，当B内核缓冲区填满，继续send，A内核缓冲区填满，继续send，则
//		若socket是阻塞模式，继续send，会阻塞在send调用处
//		当socket是非阻塞模式，继续send，不会阻塞，而是立即出错并返回，得到相关错误码（EWOULDBLOCK或EAGAIN）
//	内核缓冲区专用名词是TCP窗口，抓包时的win字段
//	当抓包显示对端TCP窗口为0后，还可以继续发送数据一段时间，则是在填充到本端缓冲区
// 
//	建立连接后，服务端没有回复数据给客户端，客户端调用recv无数据可读，则
//		阻塞模式下，会阻塞在函数调用处
//		非阻塞模式，立即返回-1，错误码为EWOULDBLOCK
// 
//	所以，非阻塞下，要注意send/recv函数的返回值
//		大于0，表示发送或接收了多少字节，但依旧需要确认，大小是否符合我们的期望值；
//		有时TCP窗口快满了，只发送/收到了部分数据，则应记录偏移，继续尝试发送
//		等于0，一般认为对端关闭了连接，同样关闭本端连接即可
//		小于0，有几种错误码，返回值都是-1，检查errno
// 				错误码						send				recv
//				EWOUDBLOCK或EAGAIN		TCP窗口太小			当前缓冲区无可读数据
// 				EINTR							被信号中断，需要重试
// 				其他									出错
// 
// 阻塞和非阻塞各自场景
//	非阻塞一般用于需要高并发多QPS场景（服务端）
//	阻塞场景
//		发送文件，文件分段，每发送一段，对端响应一次，可以单开线程，在这个线程里阻塞模式每次都是先send后recv（之前写的一个备份程序差不多就是这样）
//		AB两端只有问答模式，也就是A发送请求一定会有B的响应，同时B端不向A推送数据，则A可以使用阻塞模式
//

//
//发送0字节数据
// 一般send/recv返回0表示对端关闭了连接
// 若使用send发送数据指定len为0，返回值确实是0，但抓包会发现并没有数据包传输过程
// recv并没有接收到数据
// 即，send实际上返回值为0有两种情况，但一般是不会发送0字节数据的
// 所以send/recv都默认为对端关闭了连接
//

//
//connect函数
// 阻塞模式下，若网络状态差或地址较远，连接速度慢，则会在connect处阻塞一段时间，直到有明确的成功或失败才返回
// 虽然依赖网络通信的程序前提是连接，似乎影响不大，但还是倾向于使用非阻塞模式
// 步骤：
//	创建socket，设置非阻塞模式
//	调用connect，无论是否成功都会立即返回，返回-1不一定表示出错，错误码为EINPROGRESS则表示正在尝试
//	调用select，在指定时间内判断该socket是否可写，当可写时，说明连接成功
//

//
//连接成功后接收第一组数据
// 网络通信双方建立连接之后就把对端第一组数据接收（类似http第三次握手成功后就把包带过去？）
// Linux提供TCP_DEFER_ACCEPT，用setsockopt设置后，只有连接成功并且接收到第一个包时，accept函数才会返回
// Windows有扩展函数AcceptEx，同样的功效；连接方也可设置当连接时顺便发送第一组数据，ConnectEx
//

//
//获取当前socket对应的接收缓冲区中可读数据量
// 当非监听socket可读时，想获取缓冲区有多少数据可读（类似java.io.InputStream.available）
// Windows提供了 int ioctlsocket(SOCKET s, long cmd, u_long* argp); 通过传cmd命令为FIONREAD，可从第三个参数获取结果
// linux使用 int ioctl(int d, int request, ...); 用法基本一致（不同的是，第三个参数必须初始化为0再传递才能得到正确结果）
// 获取这个数值并不代表可以从recv或read接收之前拿到大小后将值设置为接收缓冲区大小来使用
// 因为调用ioctl后，recv之前的这段那时间，缓冲区可能又增加了新数据
// 所以这个很少用
//

//
//错误码和信号
// 在一些socket函数调用出错返回-1时，不能直接判断为失败，而是根据不同的错误码编写对应逻辑
//	错误码 EINTR 就表示被信号中断了，而不是失败，需要再次重试
// TCP通信双方AB，当A关闭连接，B继续发送数据，则根据TCP规定，B收到一个A的RST报文应答
//	继续发送数据，则系统产生SIGPIPE信号给B，告诉它连接已经断开了
//	系统对这个信号的默认处理是让B退出，但这并不友好
// TCP通信是全双工，可看为两条单工，两端各负责一条，当对端关闭，虽然含义是关闭两条
//	但本端值收到FIN，根据TCP规定，表示对端只关闭自己，虽然不再发送，但可以继续接收
//	也就是说，通信中，无法得知对端socket调用的close还是shutdown（ int shutdown(int socket, int how); how: SHUT_RD,SHUT_WR,SHUT_RDWR）
//	对一个已经收到FIN包的socket调用read/recv，缓冲区为空则返回0，也就是常说的关闭状态
//	调用send/write时，如果缓冲区没问题，则可以发送成功，对端返回RST；由于上次的发送正常，程序逻辑可能继续发送，导致产生SIGPIPE进程退出
// 后端开发中，如果因为某个客户端连接出现这种问题，整个进程结束是不合理的
//	可以捕获该信号并忽略 signal(SIGPIPE, SIG_IGN);
//	设置后，同样的逻辑，收到fin的socket调用send/write，返回RST，再次发送，返回-1，错误码是SIGPIPE，就可以在程序中处理
//

//
//poll函数（linux）
// 用于检测一组fd上的可读可写和出错事件
// int poll(pollfd* fds,
//		nfds_t nfds,
//		int timeout);
// fds 指向结构体首个元素指针
// nfds 结构体数组长度 本质是 typedef unsigned long int nfds_t
// timeout 表示超时事件，单位毫秒
// struct pollfd {
//		int fd;				//待检测事件的fd
//		short events;		//关心的事件组合，由开发者设置，告诉内核关注的事件
//		short revents;		//检测后得到事件类型，在poll返回时内核设置的，表示发生了的事件
// };
// 
// 事件宏		描述								是否可以作为输入（events）和输出（revents）参数
// POLLIN		数据可读
// POLLOUT		数据可写
// POLLRDNORM	等同POLLIN
// POLLRDBAND	优先带数据可读						   除了后三个不能作为events，其他的都通用
// POLLPRI		高优先数据可读（带外数据）
// POLLWRNORM	等同POLLOUT
// POLLWRBAND	优先带数据可写
// POLLRDHUP	对端关闭连接
// POPPHUP		挂起
// POLLERR		错误
// POLLNVAL		文件描述符没打开
// 
//对比select
// poll不要求计算最大描述符的值+1
// 处理大数量的文件描述符时速度更快
// 没有最大连接数的限制，因为存储的fd数组没有长度限制
// 只需要进行一次参数设置即可
// 
//缺陷
// 调用poll，不管有没有意义，大量的fd数组在用户态和内核态之间被整体复制
// 与select一样，函数返回后，需要遍历集合获取就绪fd
// 同时连接大量客户端某时刻可能很少处于就绪，效率线性下降
// 
//对于非阻塞connect
// 同样可以使用poll实现，和select同理，在一定时间内检测fd是否可写
// 
// 例：linux_SC下
//

//
//epoll模型
// select和poll优缺点综合一下，从linux内核2.6以后引入了更高效的epoll模型
// 
//基本用法
// 创建epollfd，使用函数 int epoll_create(int size);
// size参数在2.6.8之后不再使用，但还是需要设置一个大于0的值
// 若调用成功，则返回非负值的epollfd，否则为-1
// 使用epoll_ctl将需要检测事件的其他fd绑定（修改、解绑）epollfd
// int epoll_ctl(int epfd, int op, int fd, epoll_event* event);
// 其中
//	op 操作类型，EPOLL_CTL_ADD、EPOLL_CTL_MOD、EPOLL_CTL_DEL，增改删，删除时event可设置为null
//	event ，结构体epoll_event指针
// 函数返回0，则设置成功，失败返回-1，通过errno错误码获取原因
// 创建并设置好epollfd之后，调用epoll_wait检测事件
// int epoll_wait(int epfd, epoll_event* events, int maxevents, int timeout);
// 调用成功，则返回有事件的fd数量；返回0为超时，返回-1为失败
/*

epoll_wait逻辑
while (true)
{

	epoll_event epoll_events[1024];
	int n = epoll_wait(epollfd, epoll_events, 1024, 1000);
	if (n < 0)
	{
		//信号中断，重试
		if (errno == EINTR)
			continue;

		//出错，退出
		break;
	}

	if (n == 0)
		//超时，继续试
		continue;

	for (size_t i = 0; i < n; ++i)
	{
		if (epoll_events[i].events & EPOLLIN)
			//可读事件
		else if (epoll_events[i].events & EPOLLOUT)
			//可写事件
		else if (epoll_events[i].events & EPOLLERR)
			//异常事件
	}

}

*/
//对比epoll和poll
// 在epoll_wait成功后，返回事件数量，直接就能操作事件列表（参数events是输出参数）
// fd数量越多时，epoll效率一般更高，因为内部结构更复杂，如果数量小，不一定比poll快
// 
//epoll触发模式
// 默认为LT（Level Trigger）水平触发模式，一个事件只要有，就会一直触发
// 还有一个ET（Edge Trigger）边缘触发模式，一个事件从无到有时触发一次
// （电学术语，可将fd有数据状态看为高电平，没有则为低电平；水平触发为 低电平->高电平或处于高电平；边缘触发条件只有 低->高）
// 用socket说明
// 读事件中，水平模式下，只要有未读完数据，一直产生EPOLLIN事件，边缘模式则是只有数据来时触发一次
// 写事件中，水平模式下tcp窗口只要不饱和就会一直有EPOLLOUT事件，边缘模式则是只有窗口从饱和变为不饱和时触发一次
// 处理方式，非阻塞socket中
// 在边缘模式，触发读事件后，一定要把数据接收干净（一直recv到报错EWOULDBLOCK）
// 水平模式下，可以固定字节数收取
// 
//主流网络库的做法，见第7章
// 
// 模式总结
// LT模式读事件触发可以按需读取，不必接收干净；ET模式则需要完全读完，否则可能没机会再读，有机会也需要处理上个包没读完的部分，难度增加且响应变慢
// LT模式写事件不需要时需要及时移除，否则会一直不必要的触发，浪费CPU；ET模式下如果数据分片发送，每次需要再重新注册一次写事件以继续发送
// 两种模式各有利弊
// 
//EPOLLONESHOT选项
// 如果某个socket的epoll_event注册了该标志，则其关心的事件，只会触发一次，除非重新注册
// 在多线程同时处理某socket上事件时，为了避免数据乱序，可以减少同步逻辑的处理
// 例如，多个线程从一个socket上处理EPOLLIN事件读数据，设置ONESHOT后，一个线程处理完，再次注册，就可以保证同时只有一个线程在处理这次事件触发了
// 当然，多线程同时操作一个socket并不是很好，实际应当避免
// 
//例：linux_SC下
//

//
//高效函数 readv/writev
// 高性能服务器有一个原则：尽量减少系统调用
// 对一个文件描述符的读或者写，都是系统调用
// 有时候需要将一个文件或套接字中的数据读到多个缓冲区，或将多个缓冲区数据同时写入一个fd对应文件或套接字
// 多次调用read或write操作缓冲区，不太方便，linux提供了一系列函数完成这类工作
// readv(int fd, const struct iovec* iov, int iovcnt);
// writev(int fd, const struct iovec* iov, int iovcnt);
// preadv(int fd, const struct iovec* iov, int iovcnt, off_t offset);
// pwritev(int fd, const struct iovec* iov, int iovcnt, off_t offset);
// 参数iov是iovec数组，iovcnt是数组个数
// 调用成功返回读取或写入的总数
// iovec结构体定义为 数据起始地址和数据长度，表示一段数据
/*

多条数据写入表格示例
char* table_head = "name,age";
char* first = "yshen,26";
char* second = "ling,26";

iovec iov[3];
iov[0].iov_base = table_head;
iov[0].iov_len = strlen(table_head);
iov[1].iov_base = first;
iov[1].iov_len = strlen(first);
iov[2].iov_base = second;
iov[2].iov_len = strlen(second);

ssize_t ret = writev(file, iov, 3);

*/
// 很多网络库会使用这样的函数提高执行效率
//



int main()
{
	std::cout << "Hello World!\n";
}
