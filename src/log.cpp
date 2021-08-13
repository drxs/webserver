/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 14:02:04
 * @ Modified Time: 2021-08-13 15:40:46
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

void LOG::log(string type, string file, int line, string str){
    /* 拼接字符串 */
    string info = type + " -- " + file + ":" + std::to_string(line) + " -- " + str + "\n";
    this->buf.push_back(info);
}

void LOG::save(){
    string tmp;
    int n = 0;
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
