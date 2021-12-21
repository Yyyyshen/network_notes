// linux_server_program_05.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//信号
//

//
//信号是由用户、系统或进程发送给目标进程的信息
//通知目标进程某状态改变或系统异常
// 
//产生情况
// 前台进程中 通过特殊终端字符，如Ctrl+C发送中断信号
// 系统异常 浮点异常或非法内存访问等
// 状态变化 定时器到期引起SIGALRM信号
// kill命令或函数
// 
//服务器程序要处理（或忽略）一些常见信号，避免异常终止
//

//
//Linux信号
// 
//发送
// 一个进程给其他进程发信号可用kill函数
// int kill(pid_t pid, int sig);
// 参数 pid > 0 发给对应进程；pid = 0 发给本组其他进程；pid = -1 发给出init外所有进程；pid < -1 发给组ID为-pid中的所有进程
// 信号定义都大于0，sig取0时不发送任何信号，但可以检测目标进程是否存在
// 
//信号处理
// 目标进程收到信号时，定义一个接收函数
// typedef void (*__sighandler_t) (int);
// 一个整型参数，用于指定信号类型
// 处理函数可重入，严禁调用不安全函数
// 除了自定义处理函数，还有两种宏定义
// SIG_DFL ((__sighandler_t) 0) 表示用默认处理
// SIG_IGN ((__sighandler_t) 1) 表示做忽略处理
// 
//网络相关信号
// SIGHUP、SIGPIPE、SIGURG
// 
//中断系统调用
// 阻塞状态下接收到信号（为该信号设置了处理函数）则默认情况下系统调用被中断
// 并且errno设置为EINTR
//

//
//信号函数
//
//signal系统调用
// _sighandler_t signal(int sig, _sighandler_t _handler);
// 参数sig指出要捕获的类型
// _handler为函数指针，作为信号的处理函数
// 调用成功的返回值是原处理方式的函数指针
// 
//sigaction
// int sigaction(int sig, const struct sigaction* act, struct sigaction* oact);
// 参数sig指出捕获类型
// act指定新的处理方式，oact输出之前的处理方式
// 结构体sigaction内部主要也是含有信号处理函数指针，加上一些其他的信息和设置
//

//
//信号集
// 
//Linux使用数据结构sigset_t表示一组信号
// #define _SIGSET_NWORDS (1024 / (8 * sizeof(unsigned long int)))
// typedef struct {
//      unsigned long int __val[_SIGSET_NWORDS];
// } __sigset_t 
// 长整型数组，每个位表示一个信号（类似fd_set）
// 提供一组函数设置、修改、删除、查询信号集
// 
//进程信号掩码
// 设置或查看进程信号掩码
// int sigprocmask(int _how, _const sigset_t* _set, sigset_t* _oset);
// 
//被挂起的信号
// 设置进程信号掩码之后，被屏蔽的信号将不能被进程接收
// 给进程发送一个被屏蔽信号，则操作系统将该信号设置为被挂起
// 当取消屏蔽时，会立刻被进程接收
// 获取进程当前被挂起的信号集
// int sigpending(sigset_t* set);
// 
//要始终清楚知道进程每个运行时刻的信号掩码，以及如何适当处理捕获的信号
//多进程、多线程环境下，要以进程和线程为单位处理信号和掩码
//

//
//统一事件源
// 信号是一种异步事件
// 信号处理函数和程序主循环是两条不同的执行线路
// 信号处理函数需要尽可能快的执行完毕，确保不被屏蔽太久
// 
// 典型的方案
// 把信号主要处理逻辑放在程序主循环，信号处理函数触发时，简单的通知主循环程序接收信号，并把信号值传递
// 主程序接收到信号值，执行目标信号对应的逻辑代码
// 信号处理函数通常用管道将信号传递给主循环（同样需要I/O复用机制，用于主循环获取管道可读事件）
// 这样下来，信号事件就可和I/O事件一样去处理
// 
// 完善的I/O框架和后台程序都统一处理信号和I/O事件（libevent和xinetd等）
// 
// 例：统一事件源简单实现
// linux_src/events_processing.cpp
//

//
//网络编程相关信号
// 
//SIGHUP
// 当挂起进程的控制终端，SIGHUP信号触发
// 对于没有控制终端的网络后台程序，通常利用此信号强制服务器重读配置文件
// 如xinetd超级服务
//  接收到SIGHUP后，调用hard_reconfig
//  循环读取/etc/xinetd.d/目录下每个子配置文件，检查变化
//  某个正在运行的自服务配置文件被修改停止服务，则xinetd给子服务进程发送SIGTERM
//  若被修改开启服务，则创建新的socket将其绑定到子服务对应端口
// 
//SIGPIPE
// 向一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号
// 此信号的默认处理方式是关闭进程，对于服务器进程，应捕获并处理它，至少要忽略它
// 引起SIGPIPE信号的写操作将设置errno为EPIPE
// 可以利用I/O复用检测管道和socket读端是否已经关闭（socket对端关闭时，POLLRDHUP事件将触发）
// 
//SIGURG
// 内核通知应用带外数据到达的方式有
//  I/O复用，之前已经有例子了
//  另一种就是使用SIGURG信号
//



//
//定时器
//

//
//网络程序中另一个常用的事件是定时事件
// 比如定期检测客户连接的活动状态
// 服务器通常管理着众多定时事件，有效组织事件，按预期触发而不影响服务器主要逻辑很重要
// 可以将每个定时事件封装为定时器，并使用某种容器类数据结构串联起来
//

//
//socket选项SO_RCVTIMEO和SO_SNDTIMEO
// 用来设置接收和发送数据的超时时间，对socket相关api有效
// 可以根据系统调用返回值和errno（EAGAIN/EWOULDBLOCK、EINPROGRESS）判断超时时间是否已到
// 进而决定是否开始处理定时任务
// 
// 例：以connect为例，使用SO_SNDTIMEO选项定时
// linux_src/timeout_connect.cpp
//

//
//I/O复用系统调用的超时参数
// select的timeval
// poll的timespec
// epoll_wait的timeout参数
//

//
//SIGALRM
// 由alarm和setitimer函数设置实施闹钟，超时时触发SIGALRM信号
// 可以利用该信号的处理函数处理定时任务
// 但如果要处理多个，就需要不断触发信号
// 
//实例：处理非活动连接
// 涉及到基于升序链表的定时器实现，并应用到例子中
// 
// linux_src/timer_based_on_linkedlist.hpp
// linux_src/nonactive_conns_clean.cpp
//

//
//高性能定时器
// 
//时间轮
// 基于排序链表的定时器中，添加定时器效率偏低
// 简单的时间轮中，指针指向轮子上的一个槽（slot）以恒定速度转动，每转一步（tick）就是一个槽
// 一个tick时间称为时间轮的槽间隔si（slot interval）
// 该时间轮共有N个槽，运转一周时N*si
// 每个槽指向一条定时器链表，每条链表定时器具有相同的特征，定时时间相差N*si整数倍
// 相当于用这种关系将定时器散列到不同链表
// 
// 复杂的时间轮可能有多个轮子，每个轮子粒度不同，相邻的轮子中，精度高的转一圈，精度低的只转一个slot（像是真正的表）
//
// 例：简单时间轮实现
// linux_src/time_wheel_timer.hpp
//

int main()
{
    std::cout << "Hello World!\n";
}
