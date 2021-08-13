/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-10 19:54:18
 * @ Modified Time: 2021-08-13 15:41:04
 * @ Description  : http逻辑任务处理
 */

#include <cctype>
#include <ctime>
#include <sys/uio.h>
#include <unordered_map>

#include "../include/locker.h"
#include "../include/http_conn.h"
#include "../include/http_content_type.h"
#include "../include/log.h"

using std::string;
using std::unordered_map;

/* 定义http响应的状态信息 */
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

/* 定义服务器名称，用于填充响应字段 */
const char* server_name = "Server: WangYusong's Server / v0.3.0(Linux)\r\n";

/* ----------网站根目录---------- */
const char* doc_root = "/var/www";

/* 定义文件名,用于记录日志 */
const string this_file = "http_conn.cpp";

/* 将文件描述符设置为非阻塞
 * 每个使用Epoll ET模式的文件描述符都应该是非阻塞的，
 * 否则读写操作可能会因为没有后续事件而一直处于阻塞状态
 */
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 将fd上的EPOLLIN注册到epfd指示的epoll内核事件表中，
 * one_shoot指示是否注册EPOLLONESHOT
 * 注册EPOLLONESHOT后，一个socket连接在任一时刻只被一个线程处理
 */
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/* 将fd从epoll内核事件表中移除 */
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/* 重置fd上的事件 */
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* 用于中文字符与unicode之间的转换 */
unsigned char toHex(unsigned char x) {   
    return  x > 9 ? x + 55 : x + 48;   
}  
  
unsigned char fromHex(unsigned char x) {   
    unsigned char y = ' ';  
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;  
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;  
    else if (x >= '0' && x <= '9') y = x - '0';  
    else assert(0);  
    return y;  
}  
  
string urlEncode(const string& str) {  
    string strTemp = "";  
    size_t length = str.length();  
    for (size_t i = 0; i < length; i++)  {  
        if (isalnum((unsigned char)str[i]) || strchr("/_.-~",str[i]) != (char*)0 )  
            strTemp += str[i];  
        else if (str[i] == ' ')  
            strTemp += "+";  
        else {  
            strTemp += '%';  
            strTemp += toHex((unsigned char)str[i] >> 4);  
            strTemp += toHex((unsigned char)str[i] % 16);  
        }  
    }  
    return strTemp;  
}  
  
string urlDecode(const string& str) {  
    string strTemp = "";  
    size_t length = str.length();  
    for (size_t i = 0; i < length; i++) {  
        if (str[i] == '+') strTemp += ' ';  
        else if (str[i] == '%') {  
            assert(i + 2 < length);  
            unsigned char high = fromHex((unsigned char)str[++i]);  
            unsigned char low = fromHex((unsigned char)str[++i]);  
            strTemp += high*16 + low;  
        }  
        else strTemp += str[i];  
    }  
    return strTemp;  
}  

/* 初始化用户数量为0 */
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

/* 关闭连接 */
void http_conn::close_conn(bool real_close) {
    if(real_close && (m_sockfd != -1)) {
        removefd(m_epollfd, m_sockfd);
        log_->log("msg", this_file, __LINE__, "Connection closed.");
        m_sockfd = -1;
        m_user_count--;     /* 关闭连接时，用户数量减1 */
    }
}

/* 初始化新接受的连接 */
void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
    /* 设置端口复用 */
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();     /* 初始化连接信息 */
}

/* 初始化连接信息 */
void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_file_type = "";
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUF_SIZE);
    memset(m_write_buf, '\0', WRITE_BUF_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

/* 从状态机，用于解析一行内容 */
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    /* m_checked_idx指向m_read_buf[]中正在分析的字符，m_read_idx指向
     * m_read_buf[]中客户尾部的下一字符。m_read_buf[]中第0～m_checked_idx
     * 字符已经分析完毕，第m_checked_idx～m_read_idx-1字符由for循环分析
     */
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        /* 获取当前要分析的字符 */
        temp = m_read_buf[ m_checked_idx ];
        /* 当前为'\r'，则可能读到完整的行 */
        if (temp == '\r') {
            if ((m_checked_idx + 1) == m_read_idx) {
                /* 当前字符为最后一字节，说明行不完整 */
                return LINE_OPEN;
            }
            else if (m_read_buf[ m_checked_idx + 1 ] == '\n') {
                /* 下一字符为'\n'，说明读到完整的行 */
                m_read_buf[ m_checked_idx++ ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }
            /* 语法错误 */
            return LINE_BAD;
        }
        else if(temp == '\n') {
            /* 当前字符为'\n'，可能读到完整的行，分析前一字符是否为'\r' */
            if((m_checked_idx > 1) && (m_read_buf[ m_checked_idx - 1 ] == '\r')) {
                m_read_buf[ m_checked_idx-1 ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    /* 行数据不完整，需要继续读取 */
    return LINE_OPEN;
}

/* 循环读取客户数据，直到无数据可读或对方关闭连接 */
bool http_conn::read() {
    if(m_read_idx >= READ_BUF_SIZE) {
        return false;
    }

    int bytes_read = 0;
    while(true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUF_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }
        else if (bytes_read == 0) {
            log_->log("msg", this_file, __LINE__, "Connection closed by client.");
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

/* 获取文件类型 */
void http_conn::get_file_type(){
    string name = string(m_url);
    /* 从右向左寻找第一个'.' */
    int pos = name.rfind('.');
    m_file_type = (pos == -1) ? "default" : name.substr(pos);
}

/* 解析HTTP请求行，获得请求方法、目标url、http版本等信息 */
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    //printf("parse_request_line -- text: %s\n",text);
    m_url = strpbrk(text, " \t");
    if (! m_url) {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
    }
    else{
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");

    m_version = strpbrk(m_url, " \t");
    if (! m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (! m_url || m_url[ 0 ] != '/') {
        return BAD_REQUEST;
    }

    /* 解码链接unicode */
    string tmp = string(m_url);
    strcpy(m_url, urlDecode(tmp).c_str());
    get_file_type();
    //printf("get file type:%s\n", m_file_type.c_str());
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/* 解析http请求的头部信息 */
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
    //printf("parse_headers text: %s\n",text);
    /* 遇到空行，表示头部字段解析完毕 */
    if(text[ 0 ] == '\0') {
        if (m_method == HEAD) {
            return GET_REQUEST;
        }
        /* http请求有消息体，还需要读取消息体，状态转移至CHECK_STATE_CONTENT */
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        /* 已经获得了完整的http请求 */
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        /* 处理Connection字段 */
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        /* 处理Content-Lenght字段 */
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0) {
        /* 处理Host字段 */
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else {
        /* 其他字段暂未处理 */
        //printf("oop! unknow header %s\n", text);
    }

    return NO_REQUEST;
}

/* 并未解析HTTP请求的消息体，只是判断是否被完整读入 */
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

/* 主状态机 */
http_conn::HTTP_CODE http_conn::process_read() {
    /* 记录当前行的读取状态 */
    LINE_STATUS line_status = LINE_OK;
    /* 记录http请求的处理结果 */ 
    HTTP_CODE ret = NO_REQUEST;

    char* text = 0;

    /* 依次从缓冲区取出所有行 */
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK ))
                || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        /* 记录下一行的起始位置 */
        m_start_line = m_checked_idx;

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                /* 第一个状态：分析请求行 */ 
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                 /* 第二个状态：分析头部字段 */ 
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                /* 第三个状态：分析http请求的消息体 */ 
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                /* 服务器错误 */ 
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

/* 当获得完整且正确的http请求时，分析目标文件属性，若文件存在、
 * 有权访问、且不是目录，则mmap到m_file_address处
 */
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    string info ="file or dir: [ " + string(m_real_file) + " ] was visited! ";
    
    
    if (stat(m_real_file, &m_file_stat) < 0){   /* 目标文件不存在 */
        info += "[ not found ]";
        log_->log("msg", this_file, __LINE__, info);    /* 文件被访问,记录到日志 */
        return NO_RESOURCE;
    }

    if (! (m_file_stat.st_mode & S_IROTH)) {    /* 无访问权限 */
        info += "[ no permission ]";
        log_->log("msg", this_file, __LINE__, info);    /* 文件被访问,记录到日志 */
        return FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_file_stat.st_mode)) {         /* 目标文件为目录，访问错误 */
        info += "[ dir, failed to visit ]";
        log_->log("msg", this_file, __LINE__, info);    /* 文件被访问,记录到日志 */
        return BAD_REQUEST;
    }

    info += "[ ok ]";
    log_->log("msg", this_file, __LINE__, info);    /* 文件被访问,记录到日志 */
    /* 打开文件，mmap到m_file_address */
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/* 对内存映射区执行munmap操作 */
void http_conn::unmap() {
    if(m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

/* 写http响应 */
bool http_conn::write() {
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(true) {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            /* 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此
             * 期间服务器无法立即收到同一客户的下一请求，但可以保证连接完整性
             */
            if(errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send) {
            unmap();
            /* 发送http响应成功，根据http请求中的Connection字段决定是否关闭连接 */
            if(m_linger) {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            } 
        }
    }
}

/* 向写缓冲中写入待发送的数据 */
bool http_conn::add_response(const char* format, ...) {
    if(m_write_idx >= WRITE_BUF_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUF_SIZE - 1 - m_write_idx, format, arg_list);
    if(len >= (WRITE_BUF_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

void http_conn::add_headers(int content_len) {
    /* 获取GMT时间 */
    time_t t= time( NULL );
    char time_buf[128]={0};
    strftime( time_buf ,127 ,"%a, %d %b %Y %H:%M:%S GMT" , gmtime(&t));

    /* 在哈希表中查找当前文件类型对应的value，不存在则设置为text/plain */
    string val = (file_type_map.find(m_file_type) == file_type_map.end()) 
        ? "text/plain" : file_type_map[m_file_type];

    add_response(server_name);
    add_response("Content-Length: %d\r\n", content_len);
    add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
    add_response("Content-Type: %s; charset=utf-8\r\n", val.c_str());
    add_response("Date: %s\r\n",time_buf);
    add_response("%s", "\r\n");     /* 添加最后的空行 */
}

bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

/* 根据服务器处理HTTP请求的结果，决定返回给客户端的内容 */
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (! add_content(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (! add_content(error_400_form)) {
                return false;
            }
            break;
        }
        case NO_RESOURCE: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (! add_content(error_404_form)) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (! add_content(error_403_form)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else {
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (! add_content(ok_string)) {
                    return false;
                }
            }
        }
        default: {
            return false;
        }
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/* 由线程池中的工作线程调用，是处理http请求的入口函数 */
void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    bool write_ret = process_write(read_ret);
    if (! write_ret) {
        close_conn();
    }

    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

