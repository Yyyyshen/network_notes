//设置守护进程
//

bool daemonize()
{
    //创建子进程，关闭符进程，使程序在后台运行
    pid_t pid = fork();
    if (pid < 0)
        return false;
    else if (pid > 0)
        exit(0);

    //设置文件权限掩码，进程创建新闻间时，文件权限为mode & 0777
    umask(0);

    //创建新会话，本进程作为首领
    pid_t sid = setsid();
    if (sid < 0)
        return false;

    //切换工作目录
    if ((chdir("/")) < 0)
        return false;

    //关闭标准输入输出和错误设备
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //关闭其他已打开的文件描述符，省略。。。

    //将标准输入输出和错误重定向到/dev/null
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    return true;
}
