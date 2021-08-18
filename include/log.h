/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 13:47:39
 * @ Modified Time: 2021-08-18 08:59:32
 * @ Description  : 日志系统 头文件
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <list>
#include <ctime>
#include "locker.h"
#include "timer.h"

using std::list;
using std::string;
using std::to_string;

class LOG{
private:
    locker lock;            /* 互斥锁 */
    list<string> buf;       /* 缓冲区 */
    string file_name;       /* 日志文件名 */
    string time_r;          /* 当前系统时间 */
    int fd;                 /* 日志文件 文件描述符 */   
    my_timer* timer1;       /* 定时器,定时保存日志到文件 */
    my_timer* timer2;       /* 定时器,定时更新time_r */

public:
    LOG(string file);
    ~LOG();
    void save();            /* 将缓冲区的数据写入文件 */
    void get_time();        /* 获取系统时间，并记录至time_r */
    void log(string type, string file, int line, string str, bool flag = false);
    void set_expire(int id, int delay);     /* 更新定时器过期时间 */
};

extern LOG* log_;

#endif