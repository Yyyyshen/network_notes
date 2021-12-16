//Copyright(c) by yyyyshen 2021
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 12345
#define SERVER_BACKLOG 5
#define BUFFER_SIZE 4096    //读缓冲区大小

enum CHECK_STATE    //主状态机两个状态：分析请求行、分析头字段
{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER
};

enum LINE_STATUS    //从状态机，行的读取状态：行完整、行出错、尚未读完
{
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
};

enum HTTP_CODE  //处理结果
{
    NO_REQUEST, //请求不完整
    GET_REQUEST,//请求完整
    BAD_REQUEST,//请求错误
    FORBIDDEN_REQUEST,//无权限
    INTERNAL_ERROR,   //服务器内部错误
    CLOSED_CONNECTION //客户端已关闭连接
};

//简易回复
static const char* szret[] = {"Rquest Succ\n", "Something wrong\n"};

//各功能函数声明
HTTP_CODE
parse_content(//分析http请求入口函数
        char* buffer, 
        int& checked_index,
        CHECK_STATE& checkstate,
        int& read_index,
        int& start_line
        );

HTTP_CODE
parse_headers(//分析头字段
        char* temp
        );

HTTP_CODE
parse_requestline(//分析请求行
        char* temp,
        CHECK_STATE& checkstate
        );

LINE_STATUS
parse_line(//从状态机，用于解析一行的内容
        char* buffer,
        int& checked_index,
        int& read_index
        );

//服务器主体逻辑
void
test_http_parse()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);
    ret = listen(listenfd, SERVER_BACKLOG);
    assert(ret != -1);

    struct sockaddr_in cli_addr;
    socklen_t cli_addrlen = sizeof(cli_addr);
    int clifd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addrlen);
    if (clifd < 0)
        printf("accept failed, errno: %d\n", errno);
    else
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        int data_read = 0;//recv结果
        int read_index = 0;//当前读取了多少字节的数据
        int checked_index = 0;//当前已分析完多少字节数据
        int start_line = 0;//行在buffer中的起始位置

        //主状态机初始状态
        CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
        while (true) //循环读取数据并分析
        {
            data_read = recv(clifd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if (data_read == -1)
                break;//读取错误
            else if (data_read == 0)
                break;//对端关闭连接

            read_index += data_read;
            //分析目前已获得的数据
            HTTP_CODE result = parse_content(buffer, 
                    checked_index, checkstate, read_index, start_line);

            if (result == NO_REQUEST)
                continue;//尚未得到一个完整HTTP请求
            else if (result == GET_REQUEST)
            {
                send(clifd, szret[0], strlen(szret[0]), 0);
                break;//得到完整、正确的请求
            }
            else
            {
                send(clifd, szret[1], strlen(szret[1]), 0);
                break;//其他情况，发生错误
            }
        }
        close(clifd);
    }
    close(listenfd);
}

//主函数
int main(int argc, char* argv[])
{
    test_http_parse();

    return 0;
}

//各功能函数定义
HTTP_CODE
parse_content(
        char* buffer,
        int& checked_index,
        CHECK_STATE& checkstate,
        int& read_index,
        int& start_line
        )
{
    LINE_STATUS linestatus = LINE_OK;//记录当前行读取状态
    HTTP_CODE retcode = NO_REQUEST;//记录HTTP请求处理结果

    //主状态机，从buffer中取出完整行
    while((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK)
    {
        char* temp = buffer + start_line; //buffer起始位置开始处理
        start_line = checked_index;//记录下一行起始位置
        //检查主状态机当前状态
        switch(checkstate)
        {
            case CHECK_STATE_REQUESTLINE: //分析请求行状态
                {
                    retcode = parse_requestline(temp, checkstate);
                    if (retcode == BAD_REQUEST)
                        return BAD_REQUEST;
                    break;
                }
            case CHECK_STATE_HEADER:
                {
                    retcode = parse_headers(temp);
                    if(retcode == BAD_REQUEST)
                        return BAD_REQUEST;
                    else if (retcode == GET_REQUEST)
                        return GET_REQUEST;
                    break;
                }
            default:
                {
                    return INTERNAL_ERROR;
                }
        }
    }

    //若没有读取到一个完整行，需要继续读取
    if (linestatus == LINE_OPEN)
        return NO_REQUEST;
    else
        return BAD_REQUEST;
}

LINE_STATUS
parse_line(
        char* buffer,
        int& checked_index,
        int& read_index
        )
{
    char temp;
    //check_index指向正在分析的字节
    //read_index指向数据尾部下一个字节
    //即
    //0～check_index字节分析完毕
    //checked_index~(read_index - 1)由此分析
    for(; checked_index < read_index; ++checked_index)
    {
        //逐个字节分析
        temp = buffer[checked_index];
        //当前字符为\r，可能读到完整行
        if (temp == '\r')
        {
            //字符刚好是当前读入的最后一个，则应返回状态OPEN，表示继续读
            if ((checked_index + 1) == read_index)
                return LINE_OPEN;
            //下一个字符刚好是\n则说明行完整
            else if (buffer[checked_index + 1] == '\n')
            {
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;//除了上面两个情况，表示有问题
        }
        //字符为\n，可能是完整行
        else if(temp == '\n')
        {
            if ((checked_index > 1) && buffer[checked_index-1] == '\r')
            {
                //对应\r刚好是最后一个，继续读，读到\n，检查前一个字符
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //所有内容分析完毕，没遇到对应字符，返回LINE_OPEN，表示继续读
    return LINE_OPEN;
}

HTTP_CODE
parse_requestline(
        char* temp, 
        CHECK_STATE& checkstate
        )
{
    char* url = strpbrk(temp, " \t");
    //如果请求行中没有空白字符或者\t字符，则一定有问题
    if (!url)
        return BAD_REQUEST;
    *url++ = '\0';

    char* method = temp;
    if (strcasecmp(method, "GET") == 0) //例子仅分析GET
        printf("The request method is GET\n");
    else
        return BAD_REQUEST;

    url += strspn(url, " \t");
    char* version = strpbrk(url, " \t");
    if (!version)
        return BAD_REQUEST;
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.1") != 0)//仅支持HTTP/1.1
        return BAD_REQUEST;

    //检查url合法性
    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/')
        return BAD_REQUEST;

    printf("The request URL is: %s\n", url);
    //请求行处理完毕，状态转移到头字段分析
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE
parse_headers(
        char* temp
        )
{
    //分析头字段
    if (temp[0] == '\0')
        return GET_REQUEST;//第一个字节是空，遇到了空行，得到正确请求
    else if (strncasecmp(temp, "Host:", 5) == 0)
    {
        //例子仅处理下HOST头字段
        temp += 5;
        temp += strspn(temp, " \t");
        printf("The request host is: %s\n", temp);
    }
    else//不处理其他字段
        printf("dont handle this header: %s\n", temp);

    return NO_REQUEST;//继续
}
