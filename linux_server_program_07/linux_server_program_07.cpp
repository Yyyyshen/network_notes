// linux_server_program_07.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//服务器优化与监测
//

//
//服务器调制、调试和测试
// 调整内核参数，可通过修改文件的方式
// 调试程序可用tcpdump抓包或者gdb
// 测试工具通常有压力测试程序，模拟高并发客户请求
//

//
//最大文件描述符数
// 查看限制
// ulimit -n
// 
// 将用户级文件描述符数设置为max-file-number（临时的）
// ulimit -SHn max-file-number
// 
// 永久修改
// /etc/security/limits.conf
// * hard nofile max-file-number
// * soft nofile max-file-number
// 
// 修改系统级文件描述符（临时的）
// sysctl -w fs.file-max=max-file-number
// 永久修改
// /etc/sysctl.conf
// fs.file-max=max-file-number
// sysctl -p
//

//
//调整内核参数
// 几乎所有内核模块，都在/proc/sys下提供了某些配置文件
// 文件名就是参数名，一个配置文件对应一个内核参数，文件内容是参数值
// 
// 查看所有内核参数
// sysctl -a
// 
//目录/proc/sys/fs
// /proc/sys/fs/file-max 系统及文件描述符限制，临时修改
//  修改后，通常需要把/proc/sys/fs/inode-max设置为新file-max值的3~4倍，否则会不够用
// /proc/sys/fs/epoll/max_user_watches 用户能够向epoll内核事件表中注册的事件总量
// 
//目录/proc/sys/net
// /proc/sys/net/core/somaxconn 指定listen监听队列中能够完整连接进入ESTABLISHED状态的socket最大数目
// /proc/sys/net/ipv4/tcp_max_syn_backlog 指定listen队列中能够转移至ESTABLISHED或SYN_RCVD状态的soccket最大数目
// /proc/sys/net/ipv4/tcp_wmem 包含3个值，分别指定一个socket的TCP写缓冲区的最小、默认、最大值
// /proc/sys/net/ipv4/tcp_rmem 包含3个值，分别指定一个socket的TCP读缓冲区的最小、默认、最大值
// /proc/sys/net/ipv4/tcp_syncookies 指定是否打开TCP同步标签，同步标签通过启动cookie防止监听socket重复接收同一个地址
// 
//修改文件或用sysctl命令修改这些参数都是临时的
//永久方式是/etc/sysctl.conf加入对应参数和数值，之后执行sysctl -p
//

//
//gdb调试
// 前面的内容有重复
//

//
//压力测试
// 可使用I/O复用、多进程、多线程等技术
// 其中单纯使用I/O复用施压程度较高，因为不涉及线程和进程的调度
// 
// 例：使用epoll实现一个通用压力测试程序
// linux_src/pressure_test_use_epoll.cpp
//

int main()
{
    std::cout << "Hello World!\n";
}
