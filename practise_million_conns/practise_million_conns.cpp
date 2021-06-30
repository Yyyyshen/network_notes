// practise_million_conns.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//测试单机百万TCP连接
// 参考 飞哥开发内功修炼网络部分的文章
// https://mp.weixin.qq.com/s?__biz=MjM5Njg5NDgwNA==&mid=2247484691&idx=1&sn=6230c5bf536265ec3280008761cce0a7&scene=21#wechat_redirect
//

//TCP连接四元组
// 源IP地址、源端口、目的IP地址和目的端口
// 任意变换一个，都是一条新连接
// 
//Linux下
// 一切皆文件，当一个进程打开一个socket，会创建包括file在内的几个内核对象
// 出于安全考虑，在多个位置都限制了可打开的文件描述符的数量，包括系统级、进程级、用户进程级
// fs.file-max：当前系统可打开的最大数量
// fs.nr_open：当前系统单个进程可打开的最大数量
// nofile：每个用户的进程可打开的最大数量
// 
//按照文章过程，使用两个PD虚拟机进行测试
//我用了一台centos和一台ubuntu，整个流程在centos没问题
//在ubuntu上遇到一些问题，一一解决
// 
// 首先是修改 /etc/security/limits.conf 也就是用户级可打开的文件限制
// *  soft  nofile  1010000  
// *  hard  nofile  1010000
// 在ubuntu下，“*”星号无法兼容，需要写出用户名
// root  soft  nofile  1010000  
// root  hard  nofile  1010000
// 
// 然后配置客户端多个ip时，要注意eth的编号，由于我pd虚拟机开了两个网卡，文章脚本默认使用了eth0，而我的局域网是eth1
// 如果冲突，会导致无法ping通，手动改了脚本就解决了
// 
// 其他就没什么了，文章写的很详细
//

//一些知识点
// 一条不活跃的 TCP 连接开销仅仅只是 3 KB 多点的内存而已。现代的一台服务器都上百 GB 的内存，如果只是说并发，单机千万（C10000K）都可以
// 并发只是描述服务器端程序的指标之一，并不是全部，加上复杂业务之后，实际并发量其实并不会这么高
// 如果在你的项目实践中真的确实需要百万条 TCP 连接，那么一般来说还需要高效的 IO 事件管理。在 c 语言中，就是直接用 epoll 系列的函数来管理
//

int main()
{
    std::cout << "Hello World!\n";
}
