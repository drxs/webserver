/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 13:47:39
 * @ Modified Time: 2021-08-14 09:19:13
 * @ Description  : 日志系统 头文件
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <list>
#include <ctime>
#include "locker.h"

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
    void save();            /* 将缓冲区的数据写入文件 */
    void get_time();        /* 获取系统时间，并记录至time_r */
    
public:
    LOG(string file);
    ~LOG();
    void log(string type, string file, int line, string str, bool flag = false);
    void run();             /* 循环判断是否将缓冲区数据写入文件 */
};

extern LOG* log_;

#endif