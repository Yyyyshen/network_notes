//Copyright(c) by yyyyshen 2021
//

#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>

#define BUFFER_SIZE 64

class util_timer;//向前声明

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;//包含定时期的用户数据结构
};

//定时器结构
class util_timer
{
public:
    using this_type = util_timer;
    using time_type = time_t;
    using data_type = client_data;
public:
    util_timer()
        : prev(nullptr), next(nullptr)
    {}

public:
    time_type expire;//超时时间，这里是绝对时间
    void (*cb_func)(data_type*);//回调函数
    data_type* user_data;//处理的客户数据，有定时器执行者传递给回调函数
    this_type* prev;//双链表结构，指向前一个定时器
    this_type* next;//指向下一个定时器
};

//定时器链表，升序双链表，带头结点和尾节点
class sort_timer_lst
{
public:
    using this_type = sort_timer_lst;
    using node_type = util_timer;
public:
    sort_timer_lst()
        : head(nullptr), tail(nullptr)
    {}
    ~sort_timer_lst()
    {
        //链表销毁时，删除所有定时器
        node_type* tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
public:
    //添加定时器
    void add_timer(node_type* timer)
    {
        if (timer == nullptr) return;//参数合法性检查
        if (head == nullptr)//链表空
        {
            head = tail = timer;
            return;
        }
        //升序逻辑
        //根据特性，如果目标定时器超时时间小于头，则小于当前所有定时器，直接插入头部
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
        }
        //否则调用重载函数，通用插入方法，将其插入链表中合适位置，保证升序特性
        add_timer(timer, head);
    }

    //某个定时任务发生变化，调整对应定时器的位置（只考虑超时时间延长的情况）
    void adjust_timer(node_type* timer)
    {
        if (timer == nullptr) return;
        
        node_type* tmp = timer->next;
        //若要调整的是尾部或新的超时时间小于下一个，则不调整（只考虑延长）
        if (tmp == nullptr || (timer->expire < tmp->expire))
            return;

        //如果是头部，则取出，按通用函数逻辑重新插入
        if (timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        //不是头结点也取出，并在原位置之后插入（只考虑延长）
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    //将目标定时器timer从链表中删除
    void del_timer(node_type* timer)
    {
        if (timer == nullptr) return;
        
        //当链表中只有一个定时器，则清空
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = nullptr;
            tail = nullptr;
            return;
        }

        //如果是头结点
        if (timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }

        //如果是尾节点
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }

        //在中间，把前后连接起来，删除自己
        timer->next->prev = timer->prev;
        timer->prev->next = timer->next;
        delete timer;
    }

private:
    //重载的私有通用函数，表示将目标定时器添加到参数head之后的部分链表中
    void add_timer(node_type* timer, node_type* lst_head)
    {
        node_type* prev = lst_head;
        node_type* tmp = prev->next;

        //遍历链表，找到大于目标定时器的节点，插入其之前
        while(tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }

            prev = tmp;
            tmp = tmp->next;
        }

        if (tmp == nullptr)
        {
            //没找到，则插入尾部
            prev->next = timer;
            timer->next = nullptr;
            timer->prev = prev;
            tail = timer;
        }
    }

public:
    //SIGALRM信号被触发时，再信号处理函数中执行一次，处理链表中到期的任务
    void tick()
    {
        if (head == nullptr) return;

        time_t cur = time(NULL);//获取当前时间
        node_type* tmp = head;
        //处理每一个定时器
        while (tmp)
        {
            //遇到尚未到时间的定时器，则退出
            if (cur < tmp->expire)
                break;

            //调用回调函数，执行任务
            tmp->cb_func(tmp->user_data);
            
            //执行任务后，删掉自己，并设置头结点
            head = tmp->next;
            if (head != nullptr)//注意判空
                head->prev = nullptr;
            delete tmp;
            tmp = head;
        }
    }

private:
    node_type* head;
    node_type* tail;
};

#endif // !LST_TIMER
