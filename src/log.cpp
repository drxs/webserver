/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-13 14:02:04
 * @ Modified Time: 2021-08-18 20:56:29
 * @ Description  : 日志系统
 */

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <exception>
#include "../include/log.h"

LOG * log_ = new LOG("./log.txt");

void func_save(){   /* 定时器回调函数,写日志文件 */
    log_->save();
    log_->set_expire(1, TIMESLOT);
}

void func_time(){   /* 定时器回调函数,更新time_r */
    log_->get_time();
    log_->set_expire(2, TIMESLOT * 6);
}

LOG::LOG(string file){
    this->file_name = file;
    this->fd = open(file.c_str(),O_RDWR | O_APPEND | O_CREAT, 0644);
    if(fd == -1){
        throw std::exception();
    }
    this->timer1 = new my_timer(TIMESLOT);          /* 每10秒保存一次log到文件 */
    this->timer1->cb_func = func_save;   
    this->timer2 = new my_timer(TIMESLOT * 6);      /* 每1分钟更新一次时间 */
    this->timer2->cb_func = func_time; 
    timer_->add_timer(timer1);
    timer_->add_timer(timer2);
    get_time();
}

/* 更新定时器过期时间 */
void LOG::set_expire(int id, int delay){
    if(id == 1){
        this->timer1->expire = time(nullptr) + delay;
    }
    else{
        this->timer2->expire = time(nullptr) + delay;
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
    time_r = str;       /* 格式：[2021-08-18 20:33] */
}

void LOG::log(string type, string file, int line, string str, bool flag){
    /* 拼接字符串 */
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
    struct stat stat_r;
    if(stat(file_name.c_str(), &stat_r) < 0){   /* 文件不存在(已被删除),重新创建日志文件 */
        close(fd);
        this->fd = open(file_name.c_str(),O_RDWR | O_APPEND | O_CREAT, 0644);
        this->log("err","log.cpp",__LINE__,
            "----- The log file was not found, this file was created just now!-----", true);
    }

    string str;
    list<string> tmp_list;
    int n = 0;
    
    lock.lock();    /* 加锁 */
    for(auto& it : buf){            /* 将多个字符串拼接到一起，从而减少write系统调用的次数 */
        str += it;
        if(str.size() > 999999){    /* 字符串过长，临时保存到链表中 */
            tmp_list.push_back(str);
            str.clear();
        }
    }

    buf.clear();
    lock.unlock();      /* 解锁 */

    if(!str.empty()){   /* str字符串非空，保存到链表 */
        tmp_list.push_back(str);
    }

    for(auto& it : tmp_list){
        n = write(fd, it.c_str(), it.size());
        if(n < 0){      /* 写失败 */
            log_->log("err", "log.cpp", __LINE__, "log file write failed");
        }
    }

} 