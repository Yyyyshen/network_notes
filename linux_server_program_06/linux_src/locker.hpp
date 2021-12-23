#ifndef LOCKER_HPP
#define LOCKER_HPP

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//信号量
class sem
{
public:
    using this_type = sem;
    using sem_type = sem_t;
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
            throw std::exception();
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    
public:
    bool wait()//等待信号量
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()//增加信号量
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_type m_sem;
};

//互斥量
class locker
{
public:
    using this_type = locker;
    using lock_type = pthread_mutex_t;
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }

public:
    bool lock()//加锁
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()//解锁
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

private:
    lock_type m_mutex;
};

//条件变量
class cond
{
public:
    using this_type = cond;
    using cond_type = pthread_cond_t;
    using lock_type = pthread_mutex_t;
public:
    cond()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
        if (pthread_cond_init(&m_cond, NULL) != 0)
            throw std::exception();
    }
    ~cond()
    {
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

public:
    bool wait()//等待条件变量
    {
        int ret = 0;
        pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal()//唤醒
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

private:
    lock_type m_mutex;
    cond_type m_cond;
};

#endif// !LOCKER_HPP
