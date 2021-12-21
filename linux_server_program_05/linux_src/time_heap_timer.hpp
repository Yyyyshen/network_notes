//Copyright(c) by yyyyshen 2021
//

#ifndef TIME_HEAP_TIMER_HPP
#define TIME_HEAP_TIMER_HPP

#include <time.h>
#include <iostream>

using std::exception;

struct client_data
{
    int sockfd;
    heap_timer* timer;
};

//定时器类
class heap_timer
{
public:
    heap_timer(int delay)
    {
        expire = time(NULL) + delay;
    }
public:
    time_t expire;//到期绝对时间
    void (*cb_func)(client_data*);
    client_data* user_data;
};

//时间堆
class time_heap
{
public:
    //初始化大小为cap的空堆
    time_heap(int cap) throw (std::excepiton)
        : capacity(cap), cur_size(0)
    {
        array = new heap_timer*[capacity];
        if (!array)
            throw std::exception();
        for (int i = 0; i < capacity; ++i)
            array[i] = nullptr;
    }
    //用已有数组初始化堆
    time_heap(heap_time* init_array[], int size, int capacity) throw (std::exception)
        : cur_size(size), capacity(capacity)
    {
        if (capacity < size)
            throw std::exception();
        array = new heap_timer*[capacity];
        if (!array)
            throw std::exception();
        for (int i = 0; i < capacity; ++i)
            array[i] = nullptr;

        if (size != 0)
        {
            for (int i = 0; i < size; ++i)
                array[i] = init_array[i];
            for (int i = (cur_size - 1) / 2; i >= 0; --i)
                percolate_down(i);//下虑
        }
    }
    //销毁堆
    ~time_heap()
    {
        for (int i = 0; i < cur_size; ++i)
            delete array[i];
        delete[] array;
    }

public:
    //添加定时器
    void add_timer(heap_timer* timer) throw (std::exception)
    {
        if (timer == nullptr) return;
        if (cur_size >= capacity) resize();//空间不够时先扩容
        //从尾部插入一个空节点
        int hole = cur_size++;
        int parent = 0;
        //上虑操作
        for(; hole > 0; hole = parent)
        {
            //从空节点位置到根节点路径上的所有节点执行
            parent = (hole - 1) / 2;
            if (array[parent]->expire <= timer->expire)
                break;//已找到合适位置
            //把父节点换下来，继续上虑
            array[hole] = array[parent];
        }
        //找到合适的位置，添入新定时器
        array[hole] = timer;
    }
    //删除
    void del_timer(heap_timer* timer)
    {
        if (timer == nullptr) return;

        //仅把回调函数置空，不真正销毁，节约删除节点造成的开销，但增加了空间开销
        //这样仅有pop到该位置时真正销毁，是一种延迟销毁
        timer->cb_func = nullptr;
    }
    //获取堆顶
    heap_timer* top() const
    {
        if (empty())
            return nullptr;
        return array[0];
    }
    //从堆顶删除
    void pop_timer()
    {
        if (empty())
            return;
        if (array[0])
        {
            delete array[0];
            //删除堆顶后，把最后一个元素替换上来
            array[0] = array[--cur_size];
            //对新堆顶执行一次下虑
            percolate_down(0);
        }
    }
    //处理函数
    void tick()
    {
        heap_timer* tmp = array[0];
        time_t cur = time(NULL);
        while (!empty())
        {
            if (tmp == nullptr)
                break;
            //堆顶没有到期，则一定没有任务到期
            if (tmp->expire > cur)
                break;
            //否则执行堆顶任务
            if(array[0]->cb_func)
                array[0]->cb_func(array[0]->user_data);
            //删除堆顶
            pop_timer();
            //继续从新的堆顶遍历
            tmp = array[0];
        }
    }
    //判空
    bool empty() const
    {
        return cur_size == 0;
    }

private:
    //下虑操作，确保参数所在节点的子树有最小堆性质
    void percolate_down(int hole)
    {
        heap_timer* temp = array[hole];
        int child = 0;
        for ( ; ((hole * 2 + 1) <= (cur_size - 1)); hole = child)
        {
            child = hole * 2 + 1;
            //选左节点还是右节点
            if ((child < (cur_size - 1)) && (array[child + 1]->expire < array[child]->expire))
                ++child;
            //不符合堆特性，则交换
            if (array[child]->expire < temp->expire)
                array[hole] = array[child];
            else
                break;//符合特性
        }
        array[hole] = temp;
    }
    //堆扩容
    void resize() throw (std::exception)
    {
        //扩大一倍
        heap_timer** temp = new heap_timer*[2 * capactity];
        if (!temp)
            throw std::excption();

        for (int i = 0; i < 2 * capacity; ++i)
            temp[i] = nullptr;

        capacity = 2 * capacity;
        for(int i = 0; i < cur_size; ++i)
            temp[i] = array[i];

        delete[] array;
        array = temp;
    }

private:
    heap_timer** array;//堆数组
    int capacity;//堆数组容量
    int cur_size;//当前元素个数
};

#endif// !TIME_HEAP_TIMER_HPP
