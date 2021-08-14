/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-10 19:54:18
 * @ Modified Time: 2021-08-14 09:42:28
 * @ Description  : Web Server主程序
 */

#include <fcntl.h>

#include "../include/locker.h"
#include "../include/threadpool.h"
#include "../include/http_conn.h"
#include "../include/log.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

/* 定义文件名,用于记录日志 */
const string this_file = "main.cpp";

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    /* 注册信号捕捉函数 */
    assert(sigaction(sig, &sa, NULL) != -1);
}

void* log_thread(void* arg){
    LOG* t = reinterpret_cast<LOG*>(arg);
    t->run();
    return nullptr;
}

/* 校验 ip port 合法性 */
bool check_ip_port(const char* ip, int port){
    auto it = inet_addr(ip);
    return (it != INADDR_NONE) && (port <= 65535);
}

/* 创建守护进程 */
void daemon_func(){
    pid_t pid;
    pid = fork();
    if(pid > 0){  //父进程终止
        exit(0);    
    }
    if(pid < 0){
        printf("daemon error!\n");
        exit(1);
    }
    //子进程
    pid = setsid();   //创建新会话
    if(pid == -1){
        printf("setsid error!\n");
        exit(1);
    }
    int fd = open("/dev/null",O_RDWR);  //fd->0
    if(fd == -1){
        printf("deamon /dev/null open error\n");
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
}

int main(int argc, char* argv[]) {
    if(argc < 3) {
        printf("usage: %s ip  port\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    /* 创建日志线程  log_ 已在log.h中声明为extern */
    pthread_t log_tid;
    pthread_create(&log_tid, nullptr, log_thread, log_);
    pthread_detach(log_tid);

    /* 校验ip地址和端口号 */
    if(!check_ip_port(ip, port)){
        log_->log("err", this_file , __LINE__, "Ip address or port number is not available");
        printf("Ip address or port number is not available!\n");
        return 1;
    }

    /* 创建守护进程 */
    daemon_func();

    log_->log("msg", this_file , __LINE__, "---------- Server is running! ----------");

    /* 忽略SIGPIPE信号 */
    addsig(SIGPIPE, SIG_IGN);
    
    /* 创建线程池 */
    threadpool< http_conn >* pool = NULL;
    try {
        log_->log("msg", this_file , __LINE__, "Try to create threadpool......");
        pool = new threadpool< http_conn >;
    }
    catch(...) {
        log_->log("err", this_file , __LINE__, "Failed to create threadpool!");
        return 1;
    }
    log_->log("msg", this_file , __LINE__, "Succeed in creating threadpool!");
    
    /* 预先对每个可能的客户连接分配一个http_conn对象 */
    http_conn* users = new http_conn[MAX_FD];
    if(!users){
        log_->log("err", this_file , __LINE__, "Failed to create users[]!");
        return 1; 
    }
    //int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        log_->log("err", this_file , __LINE__, "Failed to create socket!");
    }

    /* 使close系统调用立即返回 */
    struct linger tmp = { 1, 0 };
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int ret = 0;
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    if(ret < 0){
        log_->log("err", this_file , __LINE__, "Bind error!");
    }

    ret = listen(listenfd, 5);
    if(ret < 0){
        log_->log("err", this_file , __LINE__, "Listen error!");
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while(true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            log_->log("err", this_file , __LINE__, "Epoll error!");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd) {      /* 新连接请求 */
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                if (connfd < 0) {
                    log_->log("err", this_file , __LINE__, "Accept error!");
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD) {
                    const char *info = "Internal server busy";
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    log_->log("err", this_file , __LINE__, string(info));
                    continue;
                }
                char str[16];
                string cli_info = "new client, ip: " + string(inet_ntop(AF_INET, &client_address.sin_addr.s_addr, str, sizeof(str)))
                    + ", port: " + std::to_string(ntohs(client_address.sin_port));
                log_->log("new", this_file , __LINE__, cli_info);
                /* 初始化客户连接 */
                users[connfd].init(connfd, client_address);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                /* 如果有异常，关闭客户连接 */
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN) {
                /* 根据读的结果，决定将任务添加到线程池还是关闭连接 */
                if(users[sockfd].read()) {
                    pool->append(users + sockfd);
                }
                else {
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT) {
                /* 根据写的结果，决定是否关闭连接 */
                if(!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
            else {}
        }
    }

    /* 没有实际意义，只是消除 warning: variable ‘ret’ set but not used */
    (void)ret;

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}
