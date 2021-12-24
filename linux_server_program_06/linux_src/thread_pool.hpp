#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

//线程同步要用锁
#include "locker.hpp"

//池类，模板参数为具体任务类
template<typename T>
class thread_pool
{
public:
    thread_pool(int thread_number = 8, int max_request = 10000);
    ~thread_pool();
public:
    //向线程池投入任务
    bool append(T* request);

private:
    //工作线程绑定的函数，不断从任务队列取出任务并执行
    static void* worker(void* arg);
    void run();

private:
    int m_thread_number;//池内线程数
    int m_max_requests;//队列中允许的最大请求数
    pthread_t* m_threads;//线程池数组
    std::list<T*> m_workqueue;//请求队列
    locker m_queuelocker;//保护队列的锁
    sem m_queuestat;//信号量，是否有任务要处理
    bool m_stop;//是否结束线程
};

template<typename T>
thread_pool<T>::thread_pool(int thread_number, int max_requests)
    : m_thread_number(thread_number),
    m_max_requests(max_requests),
    m_stop(false),
    m_threads(nullptr)
{
    if ((thread_number <= 0) || (max_requests <= 0))
        throw std::exception();

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();

    //把线程池创建好，并设置为脱离主线程
    for (int i = 0; i < thread_number; ++i)
    {
        //create只能传递一个静态函数
        //如果想使用类中成员，只有两个方式
        // 通过类静态对象调用，如单体模式，静态函数可通过全局唯一实例访问动态成员
        // 将类对象作为参数传递，然后在静态函数中引用，调用其动态成员
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
thread_pool<T>::~thread_pool()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool thread_pool<T>::append(T* request)
{
    //操作队列要加锁，因为被所有线程共享
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        //请求太多，暂不能处理了
        m_queuelocker.unlock();
        return false;
    }
    //入队并解锁
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//信号量，唤醒线程来处理队列
    return true;
}

template<typename T>
void* thread_pool<T>::worker(void* arg)
{
    thread_pool* pool = (thread_pool*)arg;//传了this指针过来，方便取各种数据
    pool->run();
    return pool;
}

template<typename T>
void thread_pool<T>::run()
{
    while (!m_stop)
    {
        //等待信号量，处理完一次之后再进循环，如果没有任务过来就会阻塞
        //任务入队后会唤醒
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //从队列中取出
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();//尽量减小锁的使用粒度，队列操作完了就可以释放了
        //处理请求
        if (!request)
            continue;
        request->process();
    }
}

#endif//!THREAD_POOL_HPP
