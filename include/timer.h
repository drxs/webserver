/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-15 10:21:54
 * @ Modified Time: 2021-08-18 09:39:39
 * @ Description  : 定时器 时间堆（小顶堆） 头文件
 */

#ifndef TIMER_H
#define TIMER_H

#include <ctime>
#include <vector>
#include <exception>
#include "locker.h"

#define TIMESLOT 10  /* 时间间隔,用于产生SIGALRM信号 */

using std::vector;

/* 定时器 */
class my_timer{
public:
    time_t expire;      /* 定时器生效的绝对时间 */
    void(*cb_func)();   /* 回调函数 */
public:
    my_timer(int delay) { expire = time(nullptr) + delay;  cb_func = nullptr; }
    time_t getExpire()  { return this->expire; }
};

/* 时间堆 */
class timer_heap{
private:
    vector<my_timer*> array;      /* 堆数组 */
public:     
    timer_heap() { }            /* 构造函数，初始化大小为cap的空堆 */
    ~timer_heap();              /* 析构函数，销毁时间堆 */
public:
    void add_timer(my_timer* timer) noexcept;    /* 添加定时器到堆 */
    void del_timer(my_timer* timer);    /* 删除目标定时器 */
    my_timer* top() const;              /* 获取堆顶计时器 */
    void pop_timer();                   /* 删除堆顶计时器 */
    void tick();                        /* 心搏函数 */
    //void run();                         /* 执行函数,根据信号量执行tick() */
};

#endif

extern timer_heap* timer_;