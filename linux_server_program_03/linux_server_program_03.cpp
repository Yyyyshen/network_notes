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
// UDP
// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
// ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);
// 不依赖于建立连接的socket读写，指定号发送/接收端即可
// 另外，两个函数也可以用于面向stream的socket数据读写，只要把发送接收端的socket地址参数传NULL即可，因为双方已经连接了
// 
// 通用读写
// ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);
// ssize_t sendmsg(int sockfd, struct msghdr* msg, int flags);
// 
//带外标记
// 实际中，无法预期带外数据何时到来
// 内核通知应用带外数据到达的方式
//	I/O复用产生的异常和SIGURG信号
//	用 int sockatmark(int sockfd); 获取带外数据在数据流的位置
// 
//地址信息函数
// int getsockname(int sockfd, struct sockaddr* address, socklen_t* address_len);
// int getpeername(int sockfd, struct sockaddr* address, socklen_t* address_len);
// 用于获取本端和对端的socket地址
// 
//socket选项
// int getsockopt(int sockfd, int level, int option_name, void* option_value, socklen_t* restrict option_len);
// int setsockopt(int sockfd, int level, int option_name, const void* option_value, socklen_t* option_len);
// level 指定操作哪个协议的选项		option_name 指定选项名字
// SOL_SOCKET（通用，协议无关）		SO_REUSEADDR/SO_RCVBUF/SO_SNDBUF/SO_KEEPALIVE/SO_LINGER等
// IPPROTO_IP						IP_TTL等
// IPPROTO_IPV6						IPV6_NEXTHOP等
// IPPROTO_TCP						TCP_NODELAY等
// 
// SO_REUSEADDR
//	连接正常断开后，TCP连接处于TIME_WAIT状态，端口仍在被占用，可以利用这个选项强制使用处于此状态的地址
//	int reuse = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
// SO_RCVBUF/SO_SNDBUF
//	TCP接收、发送缓冲区大小，手动设置缓冲区后，系统会将其值加倍，并且不得小于某值
// SO_RCVLOWAT/SO_SNDLOWAT
//	接收和发送缓冲区的低水位标记，被I/O复用机制调用，判断socket是否可读或可写，默认均为1字节
//	接收缓冲区中大于水位，则可读；发送缓冲区低于水位，则可写
// SO_LINGER
//	控制close在关闭TCP连接时的行为，默认下，close立即返回，TCP模块负责把发送缓冲区中残留数据发送给对方
//	若设置开启并设置滞留时间，则close行为：根据发送缓冲区是否残留数据、是否阻塞判断
// 
//网络信息API
// IP和端口号是socket地址两要素
// gethostbyname 根据主机名称获取完整信息（先本地hosts再到DNS服务器）
// gethostbyaddr 根据IP地址获取完整信息
// 
// getservbyname/getservbyport 根据名称/端口获取某个服务的完整信息（查询/etc/services文件)
// 
// getaddrinfo 通过主机名获取IP（gethostbyname）或服务（getservbyname）信息
// 
// getnameinfo 通过socket地址获取字符串表示的主机名（gethostbyaddr）、服务名（getservbyport）
//


//
//高级I/O函数
//

//
//不如基础I/O常用
// 用于创建fd的函数				pipe、dup/dup2
// 用于读写的函数				readv/writev、sendfile、mmap/munmap、splice、tee
// 用于控制I/O行为和属性的函数	fcntl
//

//
//pipe
// 创建管道，实现进程间通信
// int pipe(int fd[2]);
// 参数填入两个int型数组，成功时返回0，并将一对打开的文件描述符填入参数指向的数组
// 通过pipe函数创建的fd[0]和fd[1]分别构成管道的两端，fd[1]只能写，fd[0]只能读
// 默认下，一对fd是阻塞的，read读取空管道将被阻塞到数据可读
// 与tcp一样传输的是字节流，但不是窗口控制，是本身有一个容量限制，2.6起管道大小65536
// 此外，socket的API中有一个socketpair函数，能够创建双向管道
// int socketpair(int domain, int type, int protocol, int fd[2]);
//

//
//dup/dup2
// 有时希望把标准输入重定向到文件，或者把标准输出重定向到网络连接
// 可以通过用于复制文件描述符的函数实现
// int dup(int fd);
// int dup2(int fd_one, int fd_two);
// 函数创建一个新的文件描述符，但与原fd指向同一个文件（可能是管道、网络连接）
// dup返回的描述符总是取系统当前可用最小值，dup2类似，但返回第一个不小于two的整数值
// 
// 例：dup函数实现基本CGI服务
// linux_src/cgi_with_dup.cpp
//

//
//readv/writev
// 将数据集中进行读写，相当于简易版recvmsg/sendmsg
// ssize_t readv(int fd, const struct iovec* vector, int count);
// ssize_t writev(int fd, const struct iovec* vector, int count);
// 在HTTP应答中，应包含一个状态行、多个头字段、一个空行和响应内容
// 一般来说，前三部分是同一块内存，响应内容在另一块单独内存，要自己拼接到一起再发送
// 也可以使用writev同时写出
// 
// 例：
// linux_src/writev_test.cpp
//

//
//sendfile
// 可在两个文件描述符之间直接传递数据（完全在内核中操作，避免了用户和内核缓冲区之间的数据拷贝）
// ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count);
// offset指从in_fd文件流的哪个位置开始读，count指定传输的字节数
// 另外，in_fd必须支持mmap等函数，即必须指向真实文件，不能是socket或者管道；out_fd则必须是socket
// 所以，基本上就是完全为了网络上传输文件设计的
// 
// 例：
// linux_src/sendfile_test.cpp
//

//
//mmap/munmap
// 用mmap申请一端内存，可将这段内存作为进程通信的共享内存，也可以直接将文件映射其中；用munmap可以释放这段内存
// void* mmap(void* start, size_t length, int prot, int flags, int fd, off_t offset);
// int munmap(void *start, size_t length);
// 参数start参数可以让用户指定某特定地址作为内存的起始地址，传NULL则是系统自动分配
// length指定内存长度，prot设置内存段访问权限（PROT_READ/PROT_WRITE/PROT_EXEC/PRONT_NONE）
// flags控制内存段内容被修改后的程序行为
// fd是被映射文件对应的描述符，offset控制从文件何处开始映射
// 分配成功则返回指向目标内存的指针，失败返回MAP_FAILED （(void*) -1）
//

//
//splice
// 用于在两个文件描述符中移动数据，也是零拷贝操作（内核操作）
// ssize_t splice(int fd_in, loff_t* off_in, int fd_out, loff_t* off_out, size_t len, unsigned int flags);
// fd_in为待输入，如果是一个管道文件，则offset必须为NULL；如果不是，例如是socket，offset表示从何处开始读。输出同理
// flag控制数据如何移动，len数据长度
// 
// 例：零拷贝回显服务
// linux_src/echo_use_splice.cpp
//

//
//tee
// 在两个管道文件描述符之间复制数据，也是0拷贝；与splice不同的是，不消耗数据，源文件描述符的数据依然可以用于读操作
// sszie_t tee(int fd_in, int fd_out, size_t len, unsigned int flags);
// 除了数据是复制而不是移动，与splice含义相同
//

//
//fcntl
// file control，提供了对文件描述符的各种控制操作
// 另一个控制文件描述符属性的系统调用是ioctl，比fcntl控制更多，但POSIX规范指定fcntl为控制fd属性和行为的首选
// int fcntl(inf fd, int cmd, ...);
// 
// 例：将一个fd设置为非阻塞模式，网络编程常用
// int setnonblocking(int fd)
// {
//		int old_option = fcntl(fd, F_GETFL);//获取当前状态
//		int new_option = old_option | O_NONBLOCK;//位操作设置标志位
//		fcntl(fd, F_SETFL, new_option);//应用新状态
//		return old_option;//返回旧状态，方便以后恢复
// }
//


//
//Linux服务器程序规范
//

//
//除了网络通信，还需要考虑其他细节
// Linux服务一般是后台形式运行，又称为daemon，没有控制终端也不会受到用户输入影响，父进程通常是init（pid为1的进程）
// 通常有一套日志系统，输出日志到文件，高级一些的输出日志到专门的UDP服务器，大部分后台进程都在/var/log目录下有自己的日志目录
// 一般以某个专门的非root身份运行，mysqld、httpd、syslogd等，分别拥有自己的运行账户mysql、apache、syslog
// 通常可配置，处理很多命令行选项，配置文件一般存放在/etc目录下，比如squid服务器配置文件在/etc/squid3/squid.conf
// 通常会在启动时生成一个PID文件存入/var/run目录，记录该后台进程的PID
// 需要考虑系统资源和限制，预测自身的负荷，比如fd总数、内存量等
//

//
//系统日志
// Linux提供了一个守护进程处理系统日志，syslogd升级版rsyslogd
// rsyslogd守护进程既能接收用户进程输出日志，又能接收内核日志
// 用户通过syslog函数生成系统日志，该函数将日志输出到一个UNIX本域socket类型（AF_UNIX）文件/dev/log
// rsyslogd则监听该文件以获取用户进程输出
// 调试信息保存在/var/log/debug，普通信息在/var/log/messages，内核消息在/var/log/kern.log
// 配置文件/etc/rsyslog.conf
// 
//syslog
// 使用此函数与rsyslogd守护进程通信
// void syslog(int priority, const char* message, ...);
// 第一个参数是设施只（默认LOG_USER）与日志级别的按位或
// 下面函数可以该笔那syslog的默认输出方式，进一步结构化日志内容
// void openlog(const char* ident, int logopt, int facility);
// 还有日志过滤函数
// int setlogmask(int maskpri);
// 最后要关闭日志功能
// void closelog();
//

//
//用户信息
// 有一组函数可以获取和设置当前进程真实用户ID（UID），还有EUID、GID、EGID
// get/setuid();等
// get系列函数可以获取，set系列函数可以切换用户身份
//

//
//进程间关系
// 
//进程组
// 每个进程除了有PID信息，还有进程组PGID，可用如下函数获取
// pid_t getpgid(pid_t pid);
// 有对应的set函数可让指定进程加入进程组
// 
//会话
// 一些关联的进程组形成一个会话
// pid_t setsid(void);
// 非组首领的进程调用该函数，创建新会话，并且称为会话的首领，同时新建一个进程组，自己就是该组首领
// ps -o pid,ppid,pgid,sid,comm | less
// ps命令可查看进程、进程组、会话等关系
//

//
//系统资源限制
// 物理设备（CPU、内存），系统策略（CPU时间等），实现限制（文件最大长度）等一系列资源限制可以通过系统函数获取
// int getrlimit(int resource, struct rlimit* rlim);
//

//
//改变工作目录和根目录
// web服务器逻辑根目录一般是某/www/，并非系统根目录
// 获取进程当前工作目录和改变工作目录可用如下函数
// char* getcwd(char* buf, size_t size);
// int chdir(const char* path);
// 改变进程根目录使用
// int chroot(const char* path);
// 调用chroot之后，只是设置了根目录这个属性，并不真正切换进程当前的工作目录，仍需要使用chdir("/")来切换一次
//

//
//服务器程序后台化
// 守护进程编写遵循一定的步骤
// 
// 例：
// linux_src/daemonize_test.cpp
// 
// 实际上，Linux系统提供了该功能的函数
// int daemon(int nochdir, int noclose);
// nochdir指定是否改变工作目录，传0则设置为/根目录，否则继续使用当前工作目录
// noclose传0时，标准输入输出被重定向到/dev/null文件，否则依然使用原来设备
//

int main()
{
	test_byteorder();
}
