#ifndef HTTP_CONN_HPP
#define HTTP_CONN_HPP

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

//需要用锁
#include "locker.hpp"

class http_conn final
{
public:
    //文件名最大长度
    static const int FILENAME_LEN = 200;
    //读写缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    //http请求方法
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
    //主状态机解析客户请求时的状态
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //处理结果
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST,
    NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
    INTERNAL_ERROR, CLOSED_CONNECTION};
    //行读取状态
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};

public:
    http_conn() = default;
    ~http_conn() = default;
public:
    //初始化连接
    void init(int sockfd, const sockaddr_in& addr);
    //关闭连接
    void close_conn(bool real_close = true);
    //处理请求
    void process();
    //非阻塞读写
    bool read();
    bool write();

private:
    //初始化连接内部调用
    void init();
    //解析http请求
    HTTP_CODE process_read();
    //响应http
    bool process_write(HTTP_CODE ret);

    //使用状态机解析http请求，供process_read调用
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    //下面一组函数供process_write调用，填充http应答
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    //所有socket事件注册到一个epoll内核事件表，所以用静态
    static int m_epollfd;
    static int m_user_count;//用户数量统计

private:
    //本连接的描述符和地址
    int m_sockfd;
    sockaddr_in m_address;
    //读缓冲
    char m_read_buf[READ_BUFFER_SIZE];
    //读缓冲中已读数据之后的位置
    int m_read_idx;
    //正在分析的字符位置
    int m_checked_idx;
    //当前解析行的起始位置
    int m_start_line;
    //写缓冲
    char m_write_buf[WRITE_BUFFER_SIZE];
    //写缓冲中已发送的数据位置
    int m_write_idx;

    //主状态机所处位置
    CHECK_STATE m_check_state;
    METHOD m_method;//请求方法

    //客户请求目标文件的完整路径，内容为doc_root + m_url
    char m_real_file[FILENAME_LEN];
    char* m_url;//客户请求的文件名
    char* m_version;//协议版本
    char* m_host;//主机名
    int m_content_length;//请求消息体长度
    bool m_linger;//是否保持连接

    //请求的目标文件被mmap映射到内存中的起始位置
    char* m_file_address;
    //目标文件的状态
    struct stat m_file_stat;
    //使用writev来执行多行写操作
    struct iovec m_iv[2];
    int m_iv_count;
};

//各响应码的状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

//网站根目录
const char* doc_root = "/var/www/html";

//通用函数
static int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//非const的static成员在类外初始化
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        --m_user_count;
    }
}

void http_conn::init(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    //避免TIME_WAIT
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //加入epollguanli
    addfd(m_epollfd, sockfd, true);
    ++m_user_count;

    //内部调用，通用初始化
    init();
}

void http_conn::init()
{
    //C++11可以直接在声明时就初始化了，不用在这里写
    //但这个函数，后面多用于重置当前连接状态，命名也许可以改成reset
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//读数据，直到无数据可读或对端关闭
bool http_conn::read()
{
    //缓冲区满了
    if (m_read_idx >= READ_BUFFER_SIZE)
        return false;

    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;//等待重试
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }

        m_read_idx += bytes_read;
    }
    return true;
}

//主状态机
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
            || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        
        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
                {
                    ret = parse_request_line(text);
                    if (ret == BAD_REQUEST)
                        return BAD_REQUEST;
                    break;
                }
            case CHECK_STATE_HEADER:
                {
                    ret = parse_headers(text);
                    if (ret == BAD_REQUEST)
                        return BAD_REQUEST;
                    else if (ret == GET_REQUEST)
                        return do_request();
                    break;
                }
            case CHECK_STATE_CONTENT:
                {
                    ret = parse_content(text);
                    if (ret == GET_REQUEST)
                        return do_request();
                    line_status = LINE_OPEN;
                    break;
                }
            default:
                return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}

//从状态机，分析一行是否结束
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        //逐个检查，看是否是一行
        if (temp == '\r')
        {
            if ((m_checked_idx +1) == m_read_idx)
                return LINE_OPEN;//已经是最后一个字符了，没读完
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                //下一个是\n则为一行
                //截断方便进行处理
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            //对应\r是最后一个字符没读完情况，检查前一个字符是否是\r
            if ((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r'))
            {
                //截断方便处理
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

//解析请求行，获取方法、url、版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
        return BAD_REQUEST;
    *m_url++ = '\0';//通过\0截断获取方法
    char* method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //处理完转到头字段处理状态
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析header头信息
http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    //空行，头字段部分解析完毕
    if (text[0] == '\0')
    {
        //继续读消息体，状态转到CHECK_STATE_CONTENT状态
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则就已经是完整HTTP请求了
        return GET_REQUEST;
    }
    //处理几个关键头字段
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            m_linger = true;
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //其他不管
    }

    return NO_REQUEST;
}

//处理请求内容
http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

//处理完一个完整http请求，分析请求的文件，如果合法（存在、有权限、非目录）则
//使用mmap映射到内存地址m_file_address处，具体写操作在专门的函数进行
http_conn::HTTP_CODE http_conn::do_request()
{
    //拼接完整目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN-len-1);
    //检查状态
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode * S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    //状态没问题，映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

//文件响应完，释放映射的内存
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}

//写响应
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        //将iov内容写出去，具体设置iov在专门的函数
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1)
        {
            //写缓冲没空间，等待，单此期间无法收到同一个客户的下一个请求
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            //否则就是出错了
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send)
        {
            //发送响应成功，释放内存映射，将epoll注册事件切回读
            unmap();
            if (m_linger)
            {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

//真正往写缓冲区填充内容的函数
bool http_conn::add_response(const char* format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;//写满了

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,
            format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
        return false;
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

//响应中添加各行的函数
bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s \r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s", content);
}

//根据处理结果，决定写什么内容给客户端
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
        case INTERNAL_ERROR:
            {
                add_status_line(500, error_500_title);
                add_headers(strlen(error_500_form));
                if (!add_content(error_500_form))
                    return false;
                break;
            }
        case BAD_REQUEST:
            {
                add_status_line(400, error_400_title);
                add_headers(strlen(error_400_form));
                if (!add_content(error_400_form))
                    return false;
                break;
            }
        case NO_RESOURCE:
            {
                add_status_line(404, error_404_title);
                add_headers(strlen(error_404_form));
                if (!add_content(error_404_form))
                    return false;
                break;
            }
        case FORBIDDEN_REQUEST:
            {
                add_status_line(403, error_403_title);
                add_headers(strlen(error_403_form));
                if (!add_content(error_403_form))
                    return false;
                break;
            }
        case FILE_REQUEST:
            {
                add_status_line(200, ok_200_title);
                if (m_file_stat.st_size != 0)
                {
                    add_headers(m_file_stat.st_size);
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                    return true;
                }
                else
                {
                    const char* ok_string = "<html><body></body></html>";
                    add_headers(strlen(ok_string));
                    if (!add_content(ok_string))
                        return false;
                }
            }
        default:
            return false;
    }

    //填装内容
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

//由工作线程唤醒后调用，是所有操作的入口
void http_conn::process()
{
    //主线程收到EPOLLIN事件
    //主动调用read后，将任务投入池中，开始处理读
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    //处理好读事件后，将返回内容填充到写缓冲
    bool write_ret = process_write(read_ret);
    if (!write_ret)
        close_conn();
    //响应数据准备完毕，切换epoll注册事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
    //后续主线程在写事件处理中调用wirte，将数据写回socket，完成一次请求响应
}

#endif//!HTTP_CONN_HPP
