/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-10 19:54:18
 * @ Modified Time: 2021-08-11 09:47:45
 * @ Description  : http逻辑任务处理 头文件
 */

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include <cstring>
#include <strings.h>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "locker.h"

class http_conn{
public:
    static const int FILENAME_LEN = 200;            /* 文件名的最大长度 */
    static const int READ_BUF_SIZE = 2048;       /* 读缓冲区的大小 */
    static const int WRITE_BUF_SIZE = 1024;      /* 写缓冲区的大小 */
    
    /* HTTP请求方法，目前只支持GET */
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    
    /* 解析客户请求时，主状态机所处状态 */
    enum CHECK_STATE {  
        CHECK_STATE_REQUESTLINE = 0,    /* 正在分析请求行 */
        CHECK_STATE_HEADER,             /* 正在分析头部字段 */
        CHECK_STATE_CONTENT 
    };

    /* 服务器处理HTTP请求的结果 */
    enum HTTP_CODE {    
        NO_REQUEST,             /* 请求尚不完整，需要继续读取客户数据 */
        GET_REQUEST,            /* 获得了完整的客户请求 */
        BAD_REQUEST,            /* 客户请求有语法错误 */
        NO_RESOURCE,            /* 目标文件不存在 */
        FORBIDDEN_REQUEST,      /* 访问权限不足 */
        FILE_REQUEST,           /* GET方法资源请求 */
        INTERNAL_ERROR,         /* 服务器内部错误 */
        CLOSED_CONNECTION       /* 客户端已关闭连接 */
    }; 

    /* 从状态机三种状态（行的读取状态） */
    enum LINE_STATUS {  
        LINE_OK,        /* 读到一个完整的行 */
        LINE_BAD,       /* 行出错 */
        LINE_OPEN       /* 行数据尚且不完整 */
    };

public:
    /* 所有socket上的事件都注册到同一个epoll内核事件表中，m_epollfd设置为static */
    static int m_epollfd;
    static int m_user_count;

private:
    int m_sockfd;                       /* 该http连接的socket */
    sockaddr_in m_address;              /* 客户端的socket地址 */
    char m_read_buf[READ_BUF_SIZE];     /* 读缓冲区 */
    int m_read_idx;         /* 标记读缓冲中已经读入的客户数据的最后一个字符的下一位置 */
    int m_checked_idx;      /* 当前正在分析的字符在读缓冲区的中位置 */
    int m_start_line;       /* 当前正在解析的行的起始位置 */
    
    char m_write_buf[WRITE_BUF_SIZE];   /* 写缓冲 */
    int m_write_idx;                    /* 写缓冲中待发送的字节数 */
    
    CHECK_STATE m_check_state;          /* 主状态机当前状态 */
    METHOD m_method;                    /* 请求方法 */
    
    /* 客户请求的目标文件完整路径 */
    char m_real_file[FILENAME_LEN];
    char* m_url;            /* 目标文件的文件名 */
    char* m_version;        /* http协议版本号，只支持http/1.1 */   
    char* m_host;           /* 主机名 */
    int m_content_length;   /* http请求的消息体长度 */
    bool m_linger;          /* http请求是否要保持连接 */

    /* 客户请求的目标文件被mmap到内存的起始位置 */
    char* m_file_address;

    /* 目标文件状态，判断文件是否存在、是否为目录、是否可读、获取文件大小 */
    struct stat m_file_stat;

    /* 采用writev执行写操作，定义下面两个成员 */
    struct iovec m_iv[2];
    int m_iv_count;         /* 被写内存块的数量 */

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in& addr);   /* 初始化新接受的连接 */
    void close_conn(bool real_close = true);          /* 关闭连接 */
    void process();     /* 处理客户请求 */
    bool read();        /* 非阻塞读 */
    bool write();       /* 非阻塞写 */

private:
    void init();                        /* 初始化连接信息 */
    HTTP_CODE process_read();           /* 解析http请求 */
    bool process_write(HTTP_CODE ret);  /* 填充http应答 */

    /* 下面一组函数被process_read调用以分析http请求 */
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    /* 下面一组函数被process_write调用以填充http应答 */
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_server();
    bool add_blank_line();
    
};

#endif
