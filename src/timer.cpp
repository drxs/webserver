/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-15 10:38:39
 * @ Modified Time: 2021-08-18 09:48:19
 * @ Description  : 时间堆（小顶堆） 
 */

#include "../include/timer.h"
#include <functional>
#include <algorithm>
#include <unistd.h>

timer_heap* timer_ = new timer_heap;

timer_heap::~timer_heap(){
    for(long unsigned int i = 0; i < array.size(); ++i){
        delete array[i];
    }
    array.resize(0);
}

struct cmp{ 
  bool operator()(my_timer*& a, my_timer*& b) const{ 
    return a->expire > b->expire; 
  } 
}; 

 /* 添加定时器到堆 */
void timer_heap::add_timer(my_timer* timer) noexcept{
    if(!timer){
        return ;
    }
    array.push_back(timer);
    std::make_heap(array.begin(), array.end(), cmp());
}

void timer_heap::del_timer(my_timer* timer){
    if(!timer){
        return ;
    }
    timer->cb_func = nullptr;
}

my_timer* timer_heap::top() const{
    if(array.empty()){
        return nullptr;
    }
    return array[0];
}

void timer_heap::pop_timer(){
    if(array.empty()){
        return ;
    }
    if(array[0]){
        //delete array[0];
        //array.erase(array.begin());
        std::make_heap(array.begin(), array.end(), cmp());
    }
}

void timer_heap::tick(){
    my_timer* tmp = array[0];
    time_t cur = time(nullptr);     /* 循环处理堆中到期的定时器 */
    while(!array.empty()){
        if(!tmp){
            break;
        }
        /* 堆顶定时器没到期,则退出循环 */
        if(tmp->expire > cur){
            break;
        }
        /* 执行堆顶计时器任务 */
        if(array[0]->cb_func){
            array[0]->cb_func();
        }
        /* 删除堆顶计时器,同时生成新计时器 */
        pop_timer();
        tmp = array[0];
    }
}
