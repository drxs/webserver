/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 13:47:39
 * @ Modified Time: 2021-08-13 15:40:49
 * @ Description  : 日志系统 头文件
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <list>
#include "locker.h"

using std::list;
using std::string;

class LOG{
private:
    locker lock;            /* 互斥锁 */
    list<string> buf;       /* 缓冲区 */
    string file_name;       /* 日志文件名 */
    int fd;                 /* 日志文件 文件描述符 */
    void save();            /* 将缓冲区的数据写入文件 */
    
public:
    LOG(string file);
    ~LOG();
    void log(string type, string file, int line, string str);
    void run();             /* 循环判断是否将缓冲区数据写入文件 */
};

extern LOG* log_;

#endif