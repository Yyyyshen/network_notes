// cpp_server_dev_05.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//网络通信故障排查常用工具
//

//
//ifconfig
// 查看网卡信息，默认只显示激活的
// win下为ipconfig
//

//
//ping
// 检测目标地址网络是否畅通
// 还有个tcping，检查端口
//

//
//telnet
// 连接 ip 上具体的服务端口
// 并可以发送消息
// 可手动输入各种协议测试
//

//
//netstat
// -a 显示所有，不使用时默认不显示LISTEN
// -t 显示tcp
// -u 显示udp
// ...
//

//
//lsof
// list opened filedesciptor
// 列出已打开的文件描述符
// linux万物皆fd
// 默认输出较多
// 常用
// -p [pid] 过滤进程
// -i 显示网络信息 -P可强制显示端口号 -n强制显示IP地址 lsof -i -Pn
// 
//技巧，使用lsof恢复删除的文件
// 若删除的文件正在被某进程使用，可以使用lsof过滤查看哪个进程使用了文件
// 然后去 /proc/[pid]/fd 目录下查看，通过编号恢复文件
//

//
//nc
// 是netcat的缩写
// 可模拟客户端或服务端
// 默认使用tcp，-u选项使用udp
// 一般都使用 -v 显示详细信息
// 
// 服务器 -l
//  nc -v -l 127.0.0.1 12345
// 在本地回环地址启动监听端口12345
// 
// 客户端
//  nc -v www.baidu.com 80
// 直接加地址和端口连接
// 默认本地端口是系统随机分配的，可以使用 -p [port] 指定
// 
// 默认发送消息是 \n 作为结尾
// 如果指定 -C 选项， 则是 \r\n 作为结束标志
// 
// 收发文件
//  nc -l [ip] [port] > [file-in]
//  nc [ip] [port] < [file-out]
//

//
//curl
// 与图形化请求模拟工具PostMan一样
// 使用curl命令在linux和mac上模拟发送请求
// 
// 选项 -X 显示指定请求方式
//  curl -X GET/POST [url]
// 选项 -G（--get） 使用get请求
// 选项 -d 为post请求添加数据内容
//  curl -X POST -d 'postdata' 'url'
// 选项 -H（--header） 添加http头
// 选项 -i 表示希望在应答结果中包含HTTP头信息
// 选项 -x（--proxy） 使用代理
//  curl -x <[protocol://][user:password@]proxyhost[:port]>
//

//
//tcpdump
// 抓包工具
// 可视化用wireshark更方便
// 
// 选项 -i 指定网卡（网卡名可通过ifconfig获取）
// 选项 -X 以ASCII和十六进制形式输出捕获数据包内容（不含链路层包头，-XX 则是包含）
// 选项 -n/-nn 不要将IP（和端口）显示为别名
// 选项 -vv/-vvv 显示更详细抓包数据
// 选项 -w 抓包原始信息写入文件
// 选项 -r 读取从-w选项保存的包
// 
//例
// tcpdump -i any 'port 12345' -XX -nn -vv
// 

//
//iptables
// 防火墙规则
// 
// -F 清除已有规则
// 
// 例，丢弃本地回环地址lo网卡上的SYN包
// iptables -I INPUT -p tcp --syn -i lo -j DROP 
// 测试连接不上的情况
// 配合抓包可以看到多次发送syn尝试握手，重试时间间隔动态调整，重试次数在内核参数中设置 /proc/sys/net/ipv4/tcp_syn_retries
//

int main()
{
    std::cout << "Hello World!\n";
}
