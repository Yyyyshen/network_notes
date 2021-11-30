// cpp_server_dev_02.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//C++后端开发工具和调试知识
//

//
//ssh
// xshell（我win用的putty，没有可视界面但轻量，mac下用的FinalShell）
// 
//ftp
// FileZilla（这个与笔者习惯一致）
//

//
//makefile和cmake
// 一般使用makefile组织C/C++项目
// 而makefile不太方便，就有了cmake，可以通过CMakeLists.txt文件生成makefile文件
//

//
//使用VS管理阅读开源项目源码
// 创建新的空项目
// 将解决方案等文件放到开源项目文件夹下
// 打开解决方案，将所有文件 “包含到项目中”并 “全部保存”
//

//
//在linux环境下用gcc编译并用gdb调试
// gcc -o proj_name src_file
// 使用-g选项，生成带调试信息的程序
/*

[root@centos-linux gdb]# g++ -o hello hello.cpp
[root@centos-linux gdb]# g++ -g -o hello-g hello.cpp
[root@centos-linux gdb]# ll
total 60
-rwxr-xr-x. 1 root root 17080 Nov 30 11:37 hello
-rw-r--r--. 1 root root    97 Nov 30 11:35 hello.cpp
-rwxr-xr-x. 1 root root 34056 Nov 30 11:37 hello-g

*/
// 带调试信息的程序会更大
// gdb proj_name
// 如果没有调试信息，会输出
// Reading symbols from /root/Desktop/network/cpp_server_dev_01/gdb/hello...(no debugging symbols found)...done.
// 调试完成若没问题，一般会生成不带调试符号信息的程序，减小程序体积，提高执行效率
// strip proj_name
/*

[root@centos-linux gdb]# strip hello-g
[root@centos-linux gdb]# ll
total 40
-rwxr-xr-x. 1 root root 17080 Nov 30 11:37 hello
-rw-r--r--. 1 root root    97 Nov 30 11:35 hello.cpp
-rwxr-xr-x. 1 root root 14448 Nov 30 11:42 hello-g				# 经过strip，体积减小
[root@centos-linux gdb]# gdb hello-g
GNU gdb (GDB) Red Hat Enterprise Linux 7.6.1-120.el7			# 通过gdb验证，也确实没有了调试符号信息
Reading symbols from /root/Desktop/network/cpp_server_dev_01/gdb/hello-g...(no debugging symbols found)...done.
(gdb) quit

*/
// -g选项同样适用于makefile、cmake
// 除了-g，一般还要加上优化级别，有O0到O4五个级别，O0表示不优化
// 调试时应当使用O0，方便排查问题
// 
//gdb调试方式
// gdb filename
//	直接启动程序并开始调试
// gdb attach pid
//	调试已经在运行的进程
// gdb filename corename
//	调试core文件，定位进程崩溃问题
/*

程序崩溃时有core文件产生
linux默认不开启这个功能
使用ulimit命令查看

[root@centos-linux gdb]# ulimit -a
core file size          (blocks, -c) 0
data seg size           (kbytes, -d) unlimited
scheduling priority             (-e) 0
file size               (blocks, -f) unlimited
pending signals                 (-i) 7201
max locked memory       (kbytes, -l) 64
max memory size         (kbytes, -m) unlimited
open files                      (-n) 55000
pipe size            (512 bytes, -p) 8
POSIX message queues     (bytes, -q) 819200
real-time priority              (-r) 0
stack size              (kbytes, -s) 8192
cpu time               (seconds, -t) unlimited
max user processes              (-u) 7201
virtual memory          (kbytes, -v) unlimited
file locks                      (-x) unlimited

第一行 cor file size 为0，表示关闭生成core文件
可以通过修改其值，表示生成文件的最大允许字节数，也可以设置为unlimited不限制大小
命令参数如上，每项都有指示

[root@centos-linux gdb]# ulimit -c unlimited
[root@centos-linux gdb]# ulimit -a
core file size          (blocks, -c) unlimited
data seg size           (kbytes, -d) unlimited
scheduling priority             (-e) 0
file size               (blocks, -f) unlimited
pending signals                 (-i) 7201
max locked memory       (kbytes, -l) 64
max memory size         (kbytes, -m) unlimited
open files                      (-n) 55000
pipe size            (512 bytes, -p) 8
POSIX message queues     (bytes, -q) 819200
real-time priority              (-r) 0
stack size              (kbytes, -s) 8192
cpu time               (seconds, -t) unlimited
max user processes              (-u) 7201
virtual memory          (kbytes, -v) unlimited
file locks                      (-x) unlimited

关闭会话后，设置值还原，而一般服务器程序是长期运行的
为了方便排查问题，可以设置为永久生效
在/etc/security/limits.conf中添加一行
* soft core unlimited
可以在常用的/etc/profile中添加 ulimited -c unlimited，每次自动执行

*/
// 生成的core文件默认是 core.pid ，位置是崩溃程序所在目录
// gdb filename corename
// 查看在何处崩溃
// bt
// 使用bt命令查看崩溃时的调用堆栈
// 
// 一个问题
//	pid是程序运行时的标记，崩溃后，进程已经结束，无法确认pid对应哪个程序
//	可以在程序启动时写一个日志文件，记录自己的pid
//	也可以自定义core文件名称和目录
//		/proc/sys/kernel/core_uses_pid 内容为1表示core文件添加pid作为扩展
//		/proc/sys/kernel/core_pattern 可以设置core文件保存路径和文件名
//		例如，echo "/path/to/corefile/core-%e-%t" > /proc/sys/kernel/core_pattern
/*

以下是摘自linux-2.6.32.59\Documentation\sysctl\kernel.txt

core_pattern:

core_pattern is used to specify a core dumpfile pattern name.
. max length 128 characters; default value is "core"
. core_pattern is used as a pattern template for the output filename;
  certain string patterns (beginning with '%') are substituted with
  their actual values.
  . backward compatibility with core_uses_pid:
  If core_pattern does not include "%p" (default does not)
  and core_uses_pid is set, then .PID will be appended to
  the filename.
. corename format specifiers:
  %<NUL>  '%' is dropped
  %%  output one '%'
  %p  pid
  %u  uid
  %g  gid
  %s  signal number
  %t  UNIX time of dump
  %h  hostname
  %e  executable filename
  %<OTHER> both are dropped .
  If the first character of the pattern is a '|', the kernel will treat
  the rest of the pattern as a command to run.  The core dump will be
  written to the standard input of that program instead of to a file.
========================================================================
core_uses_pid:
The default coredump filename is "core".  By setting
core_uses_pid to 1, the coredump filename becomes core.PID.
If core_pattern does not include "%p" (default does not)
and core_uses_pid is set, then .PID will be appended to
the filename.

*/
//
// 
//gdb常用命令 
/*

GDB Layout命令

每次gdb时不知道程序跑到哪了，只能list?? 错， layout窗口才是王道！！

命令	功能
layout src	显示源码窗口
layout asm	显示汇编窗口
layout split	显示源码 & 汇编窗口
layout regs	显示汇编 & 寄存器窗口
layout next	下一个layout
layout prev	上一个layout
C-x 1	单窗口模式
C-x 2	双窗口模式
C-x a	回到传统模式
GDB 跳转执行命令

命令	功能
start	开始启动程序,并停在main第一句等待命令
step	执行下一行语句, 如语句为函数调用, 进入函数中
next	执行下一行语句, 如语句为函数调用, 不进入函数中
finish	连续运行到当前函数返回为止
continue	从当前位置继续运行程序
return	强制令当前函数返回
call func()	强制调用函数, 也可以用print func()
run	从头开始运行程序
quit	退出程序
注： call func（） 与 print func() 的区别，在于调用void函数时， call func（）没有返回值， 而print有～～

GDB调试输出命令

命令	功能
print	输出变量值 & 调用函数 & 通过表达式改变变量值
info var	查看全局 & 静态变量
info locals	查看当前函数局部变量
list	查看当前位置代码
backtrace	查看各级堆栈的函数调用及参数
set var 变量=xx	将变量赋值为xx
注：
p/x 3*i -- x for hexadecimal, o for octal, d for decimal, f for float, c for char, s for string

断点

命令	功能
b N_LINE	在第N_LINE行上设置断点
b func	在func函数上设置端点
delete breakpoints	删除断点
disable breakpoints	禁用断点
enable 断点号	启动端点
info breakpoints	查看断点列表
break foo if x>0	设置条件断点
观察点

当待观察点被读 或 被写时，程序停下来，并输出相关信息

命令	功能
watch	设置写观察点
rwatch	设置读观察点
awatch	设置读写观察点
info watchpoints	查看观察点列表
Display跟踪点

命令	功能
display var	每次停下来时,显示设置的变量var的值
undisplay	取消跟踪显示
info display	设置读写观察点
info watchpoints	查看跟踪列表

作者：rh_Jameson
链接：https://www.jianshu.com/p/6cdd79ed7dfb
来源：简书
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。

*/
// 
// 实践：调试redis 6.0.3
//	gdb操作记录.txt
// 
//实用技巧
// print 长字符串时，可能显式不全，需要设置 set print element 0 ，之后可显式完整
// 
// 有时程序自身会接收信号，例如SIGINT，也就是Ctrl+C，但gdb中Ctrl+c为中断
//	可以使用signal SIGINT模拟发送
//	还可以改变gdb信号处理方式，handle SIGINT nostop print pass，告诉gdb接收到SIGINT不要停止
// 
// 有时函数存在，但break funcname不管用，就直接通过在文件具体行添加断点代替
// 
// 普通断点、条件断点、数据断点
//	条件	break [lineNo] if [condition]
//	数据断点	watch
// 
// 自定义gdb调试命令
//	在用户根目录，添加 .gdbinit 文件
//	例，Apache Web Server源码根目录下就有一个官方提供专供调试的gdb自定义命令文件
// 
// 
//gdb图形化界面——gdb tui
// gdb调试中断后，Ctrl+X+A调出gdbtui
// 使用layout cmd/src/asm/reg 调出命令行、源码、汇编、寄存器各窗口
// 
//gdb升级版——cgdb
// 安装可能遇到的问题，缺少相关依赖包，这里总结比较全
// https://blog.csdn.net/analogous_love/article/details/53389070
// 
//VisualGDB
// VS插件，在win上用VS远程调试linux程序
//
//

int main()
{
	std::cout << "Hello World!\n";
}
