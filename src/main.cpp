/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-10 19:54:18
 * @ Modified Time: 2021-08-12 21:52:10
 * @ Description  : Web Server主程序
 */

#include <fcntl.h>

#include "../include/locker.h"
#include "../include/threadpool.h"
#include "../include/http_conn.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

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

void show_error(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}


int main(int argc, char* argv[]) {
    if(argc < 3) {
        printf("usage: %s ip  port\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    /* 忽略SIGPIPE信号 */
    addsig(SIGPIPE, SIG_IGN);
    
    /* 创建线程池 */
    threadpool< http_conn >* pool = NULL;
    try {
        pool = new threadpool< http_conn >;
    }
    catch(...) {
        return 1;
    }

    /* 预先对每个可能的客户连接分配一个http_conn对象 */
    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    //int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

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
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while(true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd) {      /* 新连接请求 */
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                if (connfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD) {
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                char str[16];
                /* 输出客户端ip地址和端口号 */
                printf("new --- new client connection, ip: %s, port: %d\n",
                    inet_ntop(AF_INET, &client_address.sin_addr.s_addr, str, sizeof(str)),
                    ntohs(client_address.sin_port));

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
