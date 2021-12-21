//Copyright(c) by yyyyshen 2021
//

#ifndef TIME_WHEEL_TIMER_HPP
#define TIME_WHEEL_TIMER_HPP

#include <time.h>

class tw_timer;

struct client_data
{
    int sockfd;
    tw_timer* timer;
};

//定时器类
class tw_timer
{
public:
    using this_type = tw_timer;
    using data_type = client_data;
public:
    tw_timer(int rot, int ts)
        : prev(nullptr), next(nullptr), rotation(rot), timer_slot(ts)
    {}

public:
    int rotation; //转动几圈生效
    int time_slot;//属于哪个槽（对应的链表）
    void (*cb_func)(data_type*);//回调
    data_type* user_data;//绑定数据
    this_type* prev;//指向前一个定时器
    this_type* next;//指向下一个定时器
};

//时间轮
class time_wheel
{
public:
    using this_type = time_wheel;
    using node_type = tw_timer;

public:
    time_wheel()
        : cur_slot(0)
    {
        for (int i = 0; i < N; ++i)
            slots[i] = nullptr;//初始化所有槽头结点
    }
    ~time_wheel()
    {
        for (int i = 0; i < N; ++i)
        {
            node_type* tmp = slots[i];
            while (tmp)
            {
                slots[i] = tmp->next;
                delete tmp;//销毁每个槽中的定时器
                tmp = slot[i];
            }
        }
    }

public:
    //根据定时值创建定时器，并插入合适的槽
    node_type* add_timer(int timeout)
    {
        if (timeout < 0) return nullptr;//参数检查

        //根据超时时间计算转动几个滴答（tick）后触发
        int ticks = 0;
        if (timeout < SI)
            ticks = 1;//不足一个槽间隔的，按一个向上折合
        else
            ticks = timeout / SI;//正常计算，按计算特性是向下折合
        //计算定时器应该在第几轮的哪个槽中
        int rotation = ticks / N;
        int ts = (cur_slot + (ticks % N)) % N;//数组用作环一般就是计算时取余
        //按计算值创建定时器
        node_type* timer = new node_type(rotation, ts);

        //定时器插入逻辑
        //如果第ts槽的链还是空的，则直接作为头结点插入
        if (slots[ts] == nullptr)
            slots[ts] = timer;
        //否则，从头部正常插入槽中链表
        else
        {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }
    //删除指定定时器
    void del_timer(node_type* timer)
    {
        if (timer == nullptr) return;
        int ts = timer->time_slot;
        //删除逻辑
        //如果刚好是槽的头，则删除后重置头
        if (slots[ts] == timer)
        {
            slots[ts] = slots[ts]->next;
            if (slots[ts])
                slots[ts]->prev = nullptr;
            delete timer;
        }
        //否则，正常删除，连接前后
        else
        {
            timer->prev->next = timer->next;//前连后
            if (timer->next) //如果不是尾部才后连前
                timer->next->prev = timer->prev;
            delete timer;
        }
    }
    //到时执行函数，执行回调后，时间轮滚动一个slot
    void tick()
    {
        //当前槽头结点
        node_type* tmp = slots[cur_slot];

        //遍历当前槽的链表
        while (tmp)
        {
            //圈数大于0，则本轮还不需要执行
            if (tmp->rotation > 0)
            {
                --tmp->rotation;
                tmp = tmp->next;
            }
            //否则，说明定时器到期，执行任务并删除定时器
            else
            {
                tmp->cb_func(tmp->user_data);
                //删除逻辑与前面函数相同
                if (tmp == slots[cur_slot])
                {
                    slots[cur_slot] = tmp->next;
                    if (slots[cur_slot])
                        slots[cur_slot]->prev = nullptr;
                    delete tmp;
                    tmp = slots[cur_slot];
                }
                else
                {
                    tmp->prev->next = tmp->next;
                    if (tmp)
                        tmp->next->prev = tmp->prev;
                    node_type* tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }

        //处理完链表所有元素，转动时间轮
        cur_slot = ++cur_slot % N;//取余表示把数组当作环
    }

private:
    static const int N = 60;//槽数量
    static const int SI = 1;//槽间隔 1s（一分钟一圈，则此轮相当于秒针）
    node_type* slots[N];//时间轮数组，每个元素指向一个定时器链表（无序）
    int cur_slot;//当前槽
};

#endif//!TIME_WHEEL_TIMER_HPP
