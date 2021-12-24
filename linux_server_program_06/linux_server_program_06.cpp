// linux_server_program_06.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//并发支持
//

//
//多进程
//

//
//fork
// 创建新进程
// pid_t fork(void);
// 每次调用返回两次（堆栈复制后fork存在于两个进程中），父进程中返回子进程PID，子进程中返回0
// 返回值是后续代码判断当前进程是父进程还是子进程的依据
// 
// fork复制当前进程，在内核进程表创建一个新的表项，很多属性与源进程相同（堆指针、栈指针和标志寄存器）
// 子进程代码与父进程完全相同，还会复制数据
// 数据的复制是写时复制，即只有任意进程对数据写操作时，复制才会发生（缺页中断后操作系统给子进程分配内存并复制父进程数据）
// 所以，涉及到内存分配，fork应注意避免没必要的数据复制
// 
// 父进程打开的fd默认在子进程中也是打开的，fd的引用计数+1（根目录、当前工作目录等变量引用计数均+1）
//

//
//exec系列函数
// 有时需要在子进程执行其他程序，替换当前进程映像
// int exec*(const char* path, const char* arg, ...);
// 参数path指定可执行文件的路径，arg可变参数传递给新程序的main函数
// 
// 一般没有返回，除非出错
// 调用成功后，函数之后的代码都不会执行了，此时程序已被exec指定的程序代替（但不会关闭打开的fd）
//

//
//处理僵尸进程
// 多进程程序中，父进程一般需要跟踪子进程退出状态
// 子进程结束运行时，内核不会立即释放进程表表项，目的是让还在运行的父进程对子进程退出信息的查询
// 在子进程结束到父进程读取其退出状态之前这段时间，子进程处于僵尸态
// 或者子进程正常运行，父进程结束，此时子进程PPID被设置为1，即init进程接管，等待子进程结束，在它退出之前，也是僵尸态
// 
// 为避免僵尸进程产生或使子进程僵尸态结束，可用函数
// pid_t wait(int* stat_loc);
// pid_t waitpid(pid_t pid, int* stat_loc, int options);
// 函数wait将阻塞进程，直到某个子进程结束运行，返回结束运行的子进程pid，并将退出状态信息存储在参数stat_loc指向的内存中
// 有一系列宏帮助解释stat_val
// 但阻塞特性不适用于服务器，而waitpid解决了这个问题，参数options为WNOHANG时，函数调用是非阻塞的
// pid指定的子进程没结束则立即返回0，退出了就返回PID
// 
// 对于非阻塞调用，应该在事件已经发生后调用才效率最高，需要得知某子进程何时退出
// 某个进程结束时，会给父进程发送SIGCHLD信号
// 可以在父进程中捕获信号，并在信号处理函数中调用waitpid函数，彻底结束子进程
//

//
//管道
// pipe常用于进程内部通信，也可用于父进程和子进程通信
// 能够使用管道在父子进程间传递数据是基于fork机制，调用后两个管道fd都保持打开
// 一对管道只能保证进程间一个方向的数据传输，如果要双向传输，需使用两个管道（可用socketpair建立全双工管道）
//

//
//信号量
// 
//信号量原语
// 多进程访问系统某资源时，要考虑同步问题，确保任一时刻只有一个进程拥有独占式访问
// 这段对共享资源访问的代码称为临界区
// 一些从语言角度（无须内核支持）的解决并发问题的方案是忙等待，即不断等待内存某位置状态变化，CPU利用率较低
// 
// 信号量是一种特殊变量，只能取自然数值并且只支持两种操作：wait和signal
// 在linux中，这两种操作常称为 P、V操作
// P(SV) 若SV大于0，则-1，值为0，则挂起进程执行 （进入临界区操作）
// V(SV) 如果有其他进程因为等待SV而挂起，唤醒之，如果没有SV+1 （退出临界区）
// 信号量取值可以是任何自然数，最常用的是二进制信号量，只能取0和1
// 
//semget
// 创建一个新的信号量集，或获取一个已存在信号量集
// int semget(key_t key, int num_sems, int sem_flags);
// 参数key为一个键值，标识一个全局唯一的信号量集，可用相同的键值获取该信号量
// num_sems指定信号集中信号数量（如果是获取已存在的集，可传0）
// sem_flags指定一组标志，低端9bit是该信号量权限（与open的mode参数含义相同）
// 调用成功返回一个正整数，是信号量集的标识符
// 
// 创建信号集时，与之关联的内核数据结构semid_ds被创建初始化
// 
//semop
// 改变信号量的值，即执行PV操作
// 几个内核变量
// unsigned short semval;   //信号量值
// unsigned short semzcnt;  //等待信号量值变0的进程数量
// unsigned short semncnt;  //等待信号值增加的进程数量
// pid_t sempid;            //最后一次执行semop操作的进程ID
// 
// int semop(int sem_id, struct sembuf* sem_ops, size_t num_sem_ops);
// struct sembuf
// {
//      unsigned short int sem_num; //信号集中信号量的编号
//      short int sem_op;           //操作类型
//      short int sem_flg;          //影响操作的行为，如IPC_NOWAIT表示无论是否操作成功都立即返回（非阻塞），SEM_UNDO在进程退出时取消本次操作
// }
// 
//semctl
// 让调用者对信号量直接进行控制
// int semctl(int sem_id, int sem_num, int command, ...);
// 
//IPC_PRIVATE
// 值为0，在semget调用时可给key参数传递此特殊值
// 无论该信号量是否已经存在，都将创建一个新的信号量
// 使用该键值创建的信号量并非进程私有
// 可以用作父子进程之间
// 
// 例：父子进程之间使用一个IPC_PRIVATE信号量同步
// linux_src/ipc_private_sig_test.cpp
//

//
//共享内存
// 是进程通信的最高效机制，不涉及进程间的数据传输
// 但需要用辅助手段同步对共享内存的访问
// 通常和其他IPC方式一起使用
// 
//shmget
// 创建一段新的共享内存，或获取一段已经存在的共享内存
// int shmget(key_t key, size_t size, int shmflg);
// 参数和semget含义基本相同
// 调用成功返回一个正整数作为标识符
// 这段共享内存所有字节初始化为0，内核数据结构shmid_ds将被创建并初始化
// 
//shmat、shmdt
// 共享内存创建/获取后，不能立即访问，要关联到进程的地址空间中
// 使用完，需要从中分离，分别用两个函数实现
// void* shmat(int shm_id, const void* shm_addr, int shmflg);
// 参数shm_addr指定关联到进程的哪块地址空间，调用成功返回共享内存被关联的地址
// int shmdt(const void* shm_addr);
// 分离，成功返回0
// 
//shmctl
// 控制共享内存的某些属性
// int shmctl(int shm_id, int command, struct shmid_ds* buf);
// 
//共享内存的POSIX方法
// 利用MAP_ANONYMOUS标志的mmap函数可以实现父子进程的匿名内存共享
// 通过打开一个文件，可以实现无关进程之间的内存共享
// Linux提供了另一种利用mmap在无关进程间共享内存的方式，这种方式无须任何文件支持，需要先创建或打开一个POSIX共享内存对象
// int shm_open(const char* name, int oflag, mode_t mode);
// 与open使用相同，name指定要打开/创建的共享内存对象，为了移植性，通常用“/somename”形式
// oflag指定创建方式（读、写、创建等）
// 调用成功时，返回一个文件描述符，可用于后续mmap的调用，从而使共享内存关联到调用进程
// 使用后，需要关闭
// int shm_unlink(const char* name);
// 编译时需要指定链接选项 -lrt
// 
//共享内存实例
// 将之前的聊天室程序改为多进程，每个子进程处理一个客户连接
// 同时，为所有客户的socket连接读缓冲设计为一块共享内存
// 
// linux_src/chat_server_multi_progress_use_shm.cpp
//

//
//消息队列
// 两个进程间可用消息队列传递二进制数据块
// 每个数据块都有特定类型，接收方可以根据类型选择性接收，而不一定像管道一样必须FIFO方式接收
// 
//msgget
// 创建一个消息队列（或获取）
// int msgget(key_t key, int msgflg);
// 创建消息队列后，内核数据结构msqid_ds将被初始化
// 
//msgsnd
// 把一条消息添加到消息队列
// int msgsnd(int msqid, const void* msg_ptr, size_t msg_sz, int msgflg);
// msg_ptr指向准备发送的消息，消息必须定义为struct msgbuf，可指定消息类型和数据块指针
// msg_sz为消息数据部分长度
// msgflg控制msgsnd行为，通常仅支持IPC_NOWAIT标志，即非阻塞
// 
//msgrcv
// 从消息队列中获取消息
// int msgrcv(int msqid, void* msg_ptr, size_t msg_sz, long int msgtype, int msgflg);
// msgtype可指定接收何种类型消息
// 
//msgctl
// 控制消息队列的某些属性
// int msgctl(int msqid, int command, struct msqid_ds* buf);
//

//
//IPC命令
// 上面几种进程间通信方式都有一个全局唯一键值key来描述一个共享资源
// 当进程调用semget、shmget、msgget，就创建了这些共享资源实例
// ipcs命令可以观察当前系统上有哪些共享资源实例
//

//
//进程间传递文件描述符
// fork调用后，父进程打开的文件描述符在子进程仍然保持打开
// 所以文件描述符可以很方便从父进程到子进程
// 注意，传递文件描述符不是传递其值，而是在接收进程创建一个新的文件描述符，并且该文件描述符和被传递fd指向内核中相同的文件表项
// 
// 那么如何把子进程打开的fd传给父进程，或者说，在不相关进程间传递fd
// Linux下，可以利用UNIX域socket在进程间传递特殊的辅助数据，实现fd的传递
// 
// 例：子进程打开一个fd，并传给父进程，父进程读取fd获取内容
// linux_src/transfer_fd_test.cpp
//



//
//多线程
//

//
//Linux线程概述
// 
//线程模型
// 线程是应用程序内独立完成任务的执行序列，根据应用程序内用户空间和内核空间，也有内核线程和用户线程
// 线程实现的几种模式
// 完全在用户空间 
//  无须内核支持，内核仍然把整个进程作为最小单位调度
//  一个进程所有执行线程共享该进程的时间片，对外表现同样的优先级
//  此时，多个用户线程对应一个内核线程（也就是进程本身）
//  速度较快，不占用额外内核资源；但多处理器系统上，一个进程中多个线程无法运行在不同CPU
// 完全由内核调度
//  用户空间线程无须执行管理任务，优缺点与完全在用户空间模式相反
// 双层调度模式
//  前两种结合，不会消耗过多内核资源，切换速度也很快，能利用多处理器
// 

// 
//线程库
// getconf GNU_LIBPTHREAD_VERSION
// Linux上默认使用NPTL
// 
//线程创建和结束
// 创建
// int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void*(*start_routine)(void*), void* arg);
// 参数thread是新线程的标识符，其他线程操作函数通过它引用新线程，类型是一个 unsigned long int
// attr用于设置属性，传递NULL表示用默认属性
// 后两个参数用于指定运行的函数和参数
// 
// 退出
// void pthread_exit(void* retaval);
// 线程函数结束时最好调用此函数，确保安全、干净的退出
// 
// 回收
// 一个进程中所有线程都可以调用pthread_join函数回收其他线程（前提是joinable）
// 即等待其他线程结束（类似于回收进程的wait/waitpid）
// int pthread_join(pthread_t thread, void** retval);
// 该函数会阻塞直到被回收的线程结束
// 
// 取消
// int pthread_cancel(pthread_t thread);
// 接收到取消请求的目标线程可以决定是否允许被取消以及如何取消
// int pthread_setcancelstate(int state, int* oldstate);
// int pthread_setcanceltype(int type, int* oldtype);
//

//
//线程属性
// pthread_attr_t结构体定义了一套完整线程属性
// #define __SIZEOF_PTHREAD_ATTR_T 36
// typedef union
// {
//      char __size[__SIZEOF_PTHREAD_ATTR_T];
//      long int __align;
// } pthread_attr_t;
// 线程库定义一系列函数操作该类型变量
// 可设置
// 线程脱离与被回收
// 堆栈起始地址和大小
// 保护区域大小（堆栈尾部分配额外空间，保护堆栈不被错误的覆盖）
// 调度参数 线程运行优先级
// 调度策略
// 优先级有效范围
//

//
//POSIX信号量
// 与多进程一样，多线程也需要考虑同步问题
// 与IPC信号量语义几乎完全相同，只是API不完全相同
// int sem_init(sem_t* sem, int pshared, unsigned int value); //pshared参数指定是局部信号量还是多进程共享信号量
// int sem_destroy(sem_t* sem);
// int sem_wait(sem_t* sem);
// int sem_trywait(sem_t* sem);
// int sem_post(sem_t* sem);
// 其中
// wait函数以原子操作将信号值-1，如果信号量为0，则阻塞，直到信号量非0（进临界区前）
// post函数以原子操作方式将信号量+1，信号值大于0则其他调用wait的线程将唤醒（出临界区前）
//

//
//互斥锁
// 可用于保护关键代码段，确保独占式访问（像一个二进制信号量）
// 进入关键代码段，需要获得互斥量并加锁，等价于信号量P操作
// 离开关键代码段，需要解锁互斥量，以唤醒其他等待此锁的线程，等价于信号量的V操作
// 
//基础API
// int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr);
// int pthread_mutex_destroy(pthread_mutex_t* mutex);
// int pthread_mutex_lock(pthread_mutex_t* mutex);
// int pthread_mutex_trylock(pthread_mutex_t* mutex);
// int pthread_mutex_unlock((pthread_mutex_t* mutex);
// 
//属性
// pshared 指定是否允许跨进程共享互斥锁
// type 指定类型（普通所、检错锁、嵌套锁、默认锁）
//

//
//条件变量
// 提供一种线程间通知的机制：当某个共享数据达到某个值，唤醒等待这个共享数据的线程
//

//
//实例：线程同步机制包装类
// 将多种同步机制分别封装
// 
// linux_src/locker.hpp
//

//
//多线程环境
//
//可重入函数
// 如果一个函数能被多个线程同时调用且不发生竞态条件，则称它线程安全（或可重入）
// 一些Linux不可重入函数如inet_ntoa等，其内部使用了静态变量，大多数都提供了对应的可重入版本
// 可重入版本（线程安全版本）原函数名加_r，如localtime对应localtime_r
// 多线程程序中，要使用可重入版本的函数
// 
//线程和进程
// 多线程程序中某个线程调用了fork，子进程不会自动创建父进程相同数量的线程，而是只拥有一个执行线程即调用fork的那个
// 而这当中，会有一个问题
// 子进程会自动继承父进程中的互斥锁及其状态
// 父进程中被加锁的互斥锁在子进程中也是锁住的，而子进程可能不知道继承来的锁状态，这个锁可能是锁住的，但是不是由调用fork那个线程锁住的
// 此时子进程再次加锁就会死锁
// 
// 一个专门函数pthread_atfork，确保fork调用后父进程和子进程都拥有一个清楚的锁状态
// int pthread_atfor(void (*prepare)(void), void (*parent)(void), void (*child)(void);
// 三个fork句柄，prepare在fork调用创建出子进程之前执行，可用来锁住父进程的锁
// parent在fork调用创建出子进程之后，fork返回之前，父进程中执行
// child在fork返回之前，在子进程中执行
// 确保在子进程中使用锁前，锁是释放状态
// 
//线程和信号
// 每个线程可以独立设置信号掩码
// pthread_sigmask(int how, const sigset_t* newmask, sigset_t* oldmask);
// 进程中所有线程共享进程的信号，所以线程库根据线程掩码决定把信号发给哪个线程，如果每个线程都单独设置信号掩码，容易逻辑错误
// 另外，处理函数也是共享的，一个线程中设置某个信号的处理函数后，会覆盖其他线程对这个信号设置的处理函数
// 所以应该在一个专门的线程处理所有信号
// 步骤：
// 主线程创建子线程之前就调用pthread_sigmask设置好掩码，所有子线程都自动继承
// 在某个专门线程调用 sigwait 等待信号并处理
// int sigwait(const sigset_t* set, int* sig);
// 
// 例：用一个线程处理所有信号
// linux_src/handle_sig_in_one_pthread.cpp
// 
// 与进程间发送信号使用kill类似，线程间也可以
// int pthread_kill(pthread_t thread, int sig);
//



//
//进程池和线程池
//

//
//动态创建子进程和子线程实现并发的缺陷
// 创建过程是耗费时间的
// 如果一个单位仅为一个客户服务，将产生大量进程（线程），切换将消耗大量CPU时间
// 动态创建子进程是当前进程的完整镜像，可能含有大量不必要的资源占用
// 
//所以，应该用 池 这个概念
// 
//进程池为例（线程池基本也都理论适用）
// 服务器开始时就预先创建一组子进程（3~10个，线程可以和CPU数量一致，做到最大化利用）
// 所有子进程都运行相同代码，有相同属性，由于是程序启动之初创建，基本没有不必要的文件描述符，相对干净
// 有新任务到来时，主进程通过一些方案（轮询或竞争队列）选择某个子进程为之服务
//

//
//处理多客户
// 考虑问题
// 
// 监听socket和连接socket是否都由主进程管理
// 若是，则主进程接收新连接，然后将socket传递出去
// 如果只管理监听，则有连接时通知，然后子进程/线程来自己接收新连接
// 
// 一个客户连接上所有任务是否始终由一个子进程/线程处理
// 如果客户任务无状态，可以考虑使用不同的，如果有上下文关系，最好用同一个
//

//
//半同步/半异步进程池
// 实现
// 将接收连接放到子进程，避免在父子进程之间传递文件描述符
// 一个客户连接上所有任务始终由一个子进程处理
// 
// linux_src/process_pool.hpp
//

//
//使用上面进程池例子实现并发CGI服务器
// 
// linux_src/cgi_use_process_pool.cpp
//

int main()
{
    std::cout << "Hello World!\n";
}
