// cpp_server_dev_03.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

//
//多线程与资源同步
//

//
//主线程和支线程
// windows下，一个进程存在多个线程时，主线程结束则支线程也会退出
//  所以常用的做法就是让主线程不退出，也就是main启动循环
// linux下，主线程退出不会影响工作线程，但进程会变僵尸进程
//  使用ps -ef查看，带有<defunct>标识的就是僵尸进程
// 
//某线程崩溃
// 理论上线程是个独立单位，有自己的上下文堆栈
// 但通常，如在linux中，一个线程崩溃可能产生segment fault错误
// 操作系统对这个错误产生的信号默认处理就是结束进程，进程销毁则其他线程不在了
//

//
//线程基本操作
// 
// 在不同系统环境下的C写法
//
#ifdef _linux_
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
void*
func(void* args)
{
	while (1)
	{
		sleep(1);
		printf("thread_run...");
	}
}
void
test_pthread()
{
	pthread_t t_id;
	pthread_create(&t_id, NULL, func, NULL);
	/*

	int pthread_create(pthread_t* thread,
		const pthread_attr_t* attr,
		void* (*start_routine)(void*),			//默认__cdecl调用
		void* arg);

	*/
}
#elif _WIN32
#include <Windows.h>
#include <stdio.h>
DWORD WINAPI ThreadProc(LPVOID lpParameters)	//__stdcall调用，函数签名指定格式
{
	while (1)
	{
		Sleep(1000);
		printf("thread_run...");
	}
}
VOID WINAPI TestThread()
{
	DWORD dwThreadID;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThreadID);
	if (hThread == NULL)
	{
		printf("thread_create failed");
		return;
	}
}
#endif

// 
// 使用C++11之后的写法
// 
#include <thread>
void
thread_proc()
{
	while (true)
	{
		auto tid = std::this_thread::get_id();
		std::cout << "tid : " << tid << " " << "thread_run..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
void
test_thread()
{
	std::thread t(thread_proc);
	//光这样使用不行，出了函数作用域后，线程对象销毁，而任务还在运行
	//必须保证线程运行期间其对象有效，否则会崩溃
	//可以使用detach分离线程
	t.detach();

	//也可以使用join，等待线程退出，但这里无限循环的函数会阻塞
	//if (t.joinable())
	//	t.join();
}
// 
//

//
//C++一种线程使用方式
// 将线程基本功能都封装到一个类中
// 把类对象实例指针作为线程函数参数
class AThread
{
public:
	using this_type = AThread;
	using sptr_type = std::shared_ptr<std::thread>;
	using bool_type = bool;
public:
	AThread()
	{
		std::cout << "AThread_ctor" << std::endl;
	}
	~AThread()
	{
		std::cout << "AThread_dtor" << std::endl;
	}
private:
	void thread_func()
	{
		while (!m_stop)
		{
			auto tid = std::this_thread::get_id();
			std::cout << "tid : " << tid << " " << "thread_run..." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
public:
	void start()
	{
		m_stop = false;
		m_spt.reset(new std::thread(&AThread::thread_func, this));	//传递this指针，就可以调用类实例成员了
		std::cout << "thread_start" << std::endl;
	}
	void stop()
	{
		m_stop = true;
		if (m_spt)
		{
			if (m_spt->joinable())
				m_spt->join();			//join等待结束线程，线程函数终止，指针自动回收，类对象回收
		}
	}

private:
	sptr_type m_spt;
	bool_type m_stop = false;
};
void
test_AThread()
{
	AThread t;
	t.start();

	std::this_thread::sleep_for(std::chrono::seconds(5));

	t.stop();
}
// 
//

//
//感觉书确实不像吹的那么好
// 书名是C++服务器开发精髓，却在开头大量篇幅讲linux和windows下不同的C api，这种内容其实像工具书一样并没有营养
// 并不是说这样不好，也许能详细了解各系统怎么使用，但跟书名标榜的C++和精髓二词并不匹配，目前看到的都是一些在网上能查到的东西
// 所以略过一部分内容，专注看C++应用
//

//
//C++线程同步对象
// C++11
// std::mutex
// std::lockguard
// std::recursive_mutex
// C++17
// std::shared_mutex 读写锁，在多个线程对共享资源读，少许写的情况下，效率更高
//

//
//使用锁经验
// 减少使用次数，无锁队列代替锁
// 明确锁范围
// 缩小使用粒度，只对需要锁的语句用锁
#include <mutex>
#include <vector>
auto func = []() {};
std::vector<decltype(func)> g_vec;
std::mutex g_mutex;
void
test_mutex()
{
	//执行任务队列
	{
		std::lock_guard<std::mutex> guard(g_mutex);
		for (decltype(auto) f : g_vec)
			f();
	}
	//此方法只能等到所有func执行完成其他线程才能操作容器，效率较低

	//可以先把需要处理的部分置换到局部变量，然后放开容器使用权，在本地继续处理逻辑
	std::vector<decltype(func)> local_vec;
	{
		std::lock_guard<std::mutex> guard(g_mutex);
		local_vec.swap(g_vec);
	}
	for (int i = 0; i < local_vec.size(); ++i)
		local_vec[i]();

}
// 避免死锁，加锁后每条路径都解锁
// 避免活锁，不要过多使用try_lock
//

//
//线程局部存储
// 除了共享数据，也有各线程局部存储的数据
// C++11引入的thread_local
// 每个线程都有一个该变量的拷贝，互不影响，一直存在直到线程退出
//

//
//线程池与队列
// 线程池是一组线程，频繁动态创建和销毁线程不如创建一组程序生命周期内不会退出的线程
// 当有任务需要执行，线程拿到任务并执行，没有任务时，线程处于阻塞或睡眠
// 需要被执行的任务必须有个存放的地方，一般是个全局队列
// 线程池中的多个线程同时操作这个队列（加锁）
// 
// 需要实现的
//	线程池创建、向队列投递任务、从队列取任务并执行
//	退出工作线程、清理任务队列、清理线程池
#include "ThreadPool.hpp"
void
test_tpool()
{
	TaskPool pool;
	pool.init();

	for (int i = 0; i < 10; ++i)
		pool.add_task(new Task);

	std::this_thread::sleep_for(std::chrono::seconds(5));
	pool.stop();
}
// 
// 其中
// 生产消费速度差不多时，可以将队列改为环形队列，节约内存
//

//
//纤程
// Windows下的概念
// 线程调度由系统内核控制，无法确定操作系统何时运行或挂起
// 对轻量任务，创建新线程消耗较大，纤程就出现了
void
test_fiber()
{
	//ConvertThreadToFiber();
	//此函数将一个线程转换为纤程模式，并获得第一个纤程
}
// 一个线程可以包含多个纤程
// 本质来说是协程
// 
//协程
// 多线程环境下，每个线程是操作系统内核对象，上下文会频繁切换
// 一个线程服务一个socket的网络编程利用率较低
// 主流的做法是利用系统提供的基于事件模式的异步模型，用少量线程服务大量网络连接和I/O
// 但一般比较复杂
// 
// 协程可认为是应用层模拟线程，避免了上下文切换
// 内部基于线程，思路是维护一组数据结构和n个线程，真正执行者还是线程
// 协程执行的代码放入队列，n个线程从中拉出来执行
// 协程切换，以Golang为例，对系统的I/O函数（linux的epoll、select，windows的IOCP）封装
// 当异步函数返回busy或blocking，将执行序列压栈，让线程拉取另一个协程代码执行
// 
// 腾讯C/C++版本的协程库libco
// https://github.com/Tencent/libco
//

int main()
{
	//test_thread();
	//test_AThread();
	test_tpool();

	while (true)
	{
		//让主线程不退出
		std::cout << "tid : " << std::this_thread::get_id() << " main_run..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
