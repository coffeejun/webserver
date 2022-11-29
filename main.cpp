#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量

// 添加文件描述符
extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);

//添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa; // sigaction是一个函数，可以用来查询或设置信号处理方式
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);                 //填充
    assert(sigaction(sig, &sa, NULL) != -1); //如果它的条件返回错误, 则终止程序执行
}

int main(int argc, char *argv[])
{
    //至少要传递端口号
    if (argc <= 1)
    {
        // basename获取基础的参数，也就是路径
        printf("usage: %s port_number\n", basename(argv[0]));
        return 1;
    }
    //文字转为数字
    int port = atoi(argv[1]);
    //对SIGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);
    //任务类是http_conn
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return 1;
    }
    //创建一个数组用于保存所有的客户端信息
    http_conn *users = new http_conn[MAX_FD];
    //创建socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    // htons是将整型变量从主机字节顺序转变成网络字节顺序
    address.sin_port = htons(port);

    // 端口复用,1才表示端口复用
    int reuse = 1;
    // setsockopt获取或者设置与某个套接字关联的选项
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    ret = listen(listenfd, 5); // 5代表请求队列中允许的最大请求数

    // 创建epoll对象，和事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    // 添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while (true)
    {

        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {

            int sockfd = events[i].data.fd;
            //有客户端连接进来
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    //目前连接数满了
                    //给客户端写一个信息，服务器内容正忙
                    close(connfd);
                    continue;
                }
                //将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {

                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {

                if (users[sockfd].read()) //一次性把所有数据都读完
                {
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {

                if (!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}