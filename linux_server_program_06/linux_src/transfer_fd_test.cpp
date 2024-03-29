//Copyright(c) by yyyyshen 2021
//

#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static const int CONTROL_LEN = CMSG_LEN(sizeof(int));

//发送fd，参数fd为UNIX域socket，第二个参数为待发送fd
void send_fd(int fd, int fd_to_send)
{
	struct iovec iov[1];
	struct msghdr msg;
	char buf[0];
	/*	关于长度为0的char数组

		struct MyData
		{
			int nLen;
			char data[0];
		};

		在结构中，data是一个数组名；但该数组没有元素；
		该数组的真实地址紧随结构体MyData之后，而这个地址就是结构体后面数据的地址
		（如果给这个结构体分配的内容大于这个结构体实际大小，后面多余的部分就是这个data的内容）；这种声明方法可以巧妙的实现C语言里的数组扩展。
		
		实际用时采取这样：
		struct MyData *p = (struct MyData *)malloc(sizeof(struct MyData )+strlen(str))
		这样就可以通过p->data 来操作这个str。

	*/

	iov[0].iov_base = buf;
	iov[0].iov_len = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	cmsghdr cm;
	cm.cmsg_len = CONTROL_LEN;
	cm.cmsg_level = SOL_SOCKET;
	cm.cmsg_type = SCM_RIGHTS;
	*(int*)CMSG_DATA(&cm) = fd_to_send;
	msg.msg_control = &cm;//辅助数据
	msg.msg_controllen = CONTROL_LEN;

	sendmsg(fd, &msg, 0);
}

//接收fd
int recv_fd(int fd)
{
	struct iovec iov[1];
	struct msghdr msg;
	char buf[0];
	iov[0].iov_base = buf;
	iov[0].iov_len = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	cmsghdr cm;
	msg.msg_control = &cm;
	msg.msg_controllen = CONTROL_LEN;

	recvmsg(fd, &msg, 0);
	int fd_to_read = *(int*)CMSG_DATA(&cm);
	return fd_to_read;
}

int main()
{
	int pipefd[2];
	int fd_to_pass = 0;
	int ret = socketpair(PF_UNIX, SOCK_DGRAM, 0, pipefd);
	assert(ret != -1);

	pid_t pid = fork();

	if (pid == 0)
	{
		//子进程传递fd
		close(pipefd[0]);//关闭自己不需要的一端
		fd_to_pass = open("ipc_private_sig_test.cpp", O_RDWR, 0666);
		send_fd(pipefd[1], (fd_to_pass > 0) ? fd_to_pass : 0);
		close(fd_to_pass);
		exit(0);
	}

	//父进程接收fd
	close(pipefd[1]);
	fd_to_pass = recv_fd(pipefd[0]);
	char buf[1024];
	memset(buf, '\0', 1024);
	read(fd_to_pass, buf, 1023);
	printf("i got fd %d and data %s\n", fd_to_pass, buf);
	close(fd_to_pass);
}
