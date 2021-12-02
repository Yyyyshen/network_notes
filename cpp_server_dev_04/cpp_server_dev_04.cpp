// cpp_server_dev_04.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

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
// 放在项目其他文件下
//

int main()
{
	std::cout << "Hello World!\n";
}
