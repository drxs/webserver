/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 14:02:04
 * @ Modified Time: 2021-08-14 09:19:48
 * @ Description  : 日志系统
 */

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <exception>
#include "../include/log.h"

LOG * log_ = new LOG("./log.txt");

LOG::LOG(string file){
    this->file_name = file;
    this->fd = open(file.c_str(),O_RDWR | O_APPEND | O_CREAT, 0644);
    if(fd == -1){
        throw std::exception();
    }
}

LOG::~LOG(){
    /* 析构之前,将缓冲区的数据写入文件 */
    this->save();
    /* 关闭日志文件 */
    close(this->fd);
}

void LOG::get_time(){
    time_t now = time(nullptr);
	tm *ltm = localtime(&now);
    string year = to_string(1900 + ltm->tm_year);
    string month = (1 + ltm->tm_mon) < 10 ? "0" + to_string(1 + ltm->tm_mon) : to_string(1 + ltm->tm_mon);
    string day = ltm->tm_mday < 10 ? "0" + to_string(ltm->tm_mday) : to_string(ltm->tm_mday);
    string hour = ltm->tm_hour < 10 ? "0" + to_string(ltm->tm_hour) : to_string(ltm->tm_hour);
    string mintue = ltm->tm_min < 10 ? "0" + to_string(ltm->tm_min) : to_string(ltm->tm_min);
	string str = "[" + year + "-" + month + "-" + day + " " + hour + ":" +  mintue  + "]";
    time_r = str;
}

void LOG::log(string type, string file, int line, string str, bool flag){
    /* 拼接字符串 */
    get_time();
    string info = time_r + " -- " + type + " -- " + file + ":" + std::to_string(line) + " -- " + str + "\n";
    if(flag){
        lock.lock();
        this->buf.push_front(info);
    }
    else{
        lock.lock();
        this->buf.push_back(info);
    }
    lock.unlock();
}

void LOG::save(){
    string tmp;
    int n = 0;
    struct stat stat_r;
    if(stat(file_name.c_str(), &stat_r) < 0){   /* 文件不存在(已被删除),重新创建日志文件 */
        close(fd);
        this->fd = open(file_name.c_str(),O_RDWR | O_APPEND | O_CREAT, 0644);
        this->log("err","log.cpp",__LINE__,
            "----- The log file was not found, this file was created just now!-----");
    }
    lock.lock();    /* 写文件之前加锁 */
    while(!buf.empty()){
        tmp = buf.front();
        buf.pop_front();
        n = write(fd, tmp.c_str(), tmp.size());
        if(n < 0){      /* 写失败,退出循环 */
            break;
        }
    }
    lock.unlock();  /* 写完文件后解锁 */
}

void LOG::run(){
    while(true){
        sleep(4);       /* 每4秒写入文件一次 */
        if(!buf.empty()){
            this->save();
        }
    }
}
