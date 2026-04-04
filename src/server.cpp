#include <stdio.h>  // 标准输入输出：printf, perror（打印错误信息）
#include <stdlib.h> // 标准库：malloc, free, exit（这里用来处理异常退出）
#include <string.h> // 字符串操作：memset, strlen（这里用来清空缓冲区）
#include <unistd.h> // Unix 标准函数：close, read, write（关闭文件描述符、读写数据）

// 套接字核心：socket, bind, listen, accept, send, recv
// 作用：创建 TCP 连接、监听、接受客户端、收发数据
#include <sys/socket.h> 

// IP地址结构体：sockaddr_in（存放IP、端口、协议族）
// 作用：给服务器绑定地址端口用
#include <netinet/in.h> 

// 文件控制：fcntl
// 作用：把 socket 设置为【非阻塞模式】，这是 epoll 必须的
#include <fcntl.h>      

// 错误号：errno
// 作用：系统调用失败时，告诉你为什么失败（EAGAIN 等）
#include <errno.h>  

// epoll 核心头文件
// 作用：提供 epoll_create1, epoll_ctl, epoll_wait 三个高并发神器
#include <sys/epoll.h>  

#include "server.h"

// ===================== 宏定义 =====================
// 最大同时监听的事件数量（一次 epoll_wait 最多返回多少个活跃连接）
#define MAX_EVENTS 1024

//读取客户端数据的缓冲区大小
#define BUF_SIZE 1024

// ===================== 函数 =====================

/**
 * @brief 设置文件描述符为【非阻塞模式】（游双书标准写法）
 * @param fd 要设置的 socket 文件描述符
 * @return 原来的状态标志
 * 作用：让 accept / read / write 不会卡住程序
 */

int setnonblocking(int fd)
{
    //第一步:获取fd原来的状态
    int old_flag = fcntl(fd, F_GETFL);

    //第二步:在原来的状态上 加上 非阻塞标志O_NONBLOCK
    int new_flag = old_flag | O_NONBLOCK;

    //第三步:把新状态设置回去
    fcntl(fd, F_SETFL, new_flag);

    return new_flag;
}

/**
 * @brief 将一个 fd 添加到 epoll 监听红黑树中，监听【读事件】
 * @param epoll_fd epoll 实例的文件描述符
 * @param fd 要监听的 socket（监听fd 或 客户端fd）
 * 作用：告诉内核：我要监听这个 fd，有数据时通知我
 */

static void epollAddFd(int epoll_fd, int fd){
    //定义epoll事件结构体
    epoll_event ev;

    //EPOLLIN = 监听【可读事件】（客户端发来数据）
    ev.events = EPOLLIN ;

    //把要监听的fd存进去，内核触发事件时会返回给我们
    ev.data.fd = fd;

    //EPOLL_CTL_ADD = 添加fd到epoll树上
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    //✅ 关键：所有交给 epoll 管理的 fd 必须是非阻塞
    setnonblocking(fd);

}

/**
 * @brief epoll 高并发服务器主函数（Reactor 模型）
 * 流程：创建socket -> 绑定 -> 监听 -> epoll创建 -> 事件循环
 */

void runServer(uint16_t ports){
    // ============= 1. 创建 TCP 监听 socket =============
    // AF_INET = IPv4
    // SOCK_STREAM = TCP 协议
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        return;
    }

    // 设置端口复用：服务器重启后可以立刻绑定端口，不会提示“地址已使用”
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ============= 2. 绑定 IP + 端口 =============
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ports);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        close(sockfd);
        return ;
    }

    // ============= 3. 开始监听 =============
    // 5 = 全连接队列长度（最多等待处理的连接数）
    if(listen(sockfd, 100) < 0){
        perror("listen");
        close(sockfd);
        return;
    }
    

    // ============= 4. 创建 epoll 实例 =============
    // epoll_create1(0) → 创建一个 epoll 句柄（内核会创建一张红黑树）
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0){
        perror("epoll_create");
        close(sockfd);
        return;
    }

    // ============= 5. 把【监听fd】加入 epoll =============
    epollAddFd(epoll_fd, sockfd);

    // 用来接收 epoll_wait 返回的活跃事件
    epoll_event events[MAX_EVENTS];

    printf("Epoll高并发服务启动：8080端口\n");

    // ============= 6. Reactor 主事件循环（核心！）=============
    while(true){
        /**
         * @brief 阻塞等待事件（没有事件就休眠，不占CPU）
         * @param -1 表示无限等待
         * @return nready 活跃的文件描述符数量
         */
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(nready <= 0) continue;


        // ============= 7. 遍历所有活跃事件 =============
        for(int i = 0; i < nready; i++){
            // 拿到触发事件的文件描述符
            int fd = events[i].data.fd;
            // ======================
            // 事件类型1：监听fd有新客户端连接
            // ======================
            if(fd == sockfd){
                sockaddr_in cli_addr;
                socklen_t cli_len = sizeof(cli_addr);

                //接收新连接
                int conn_fd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);
                if(conn_fd < 0) continue;


                //把新客户端fd加入epoll监听
                epollAddFd(epoll_fd, conn_fd);

            }

            // ======================
            // 事件类型2：客户端发来数据了（可读事件）
            // ======================
            else{
                char buf[BUF_SIZE] = {0};

                // 非阻塞读取客户端数据
                ssize_t n = read(fd, buf, BUF_SIZE -1);

                // 客户端关闭连接 或 发生错误
                if(n <= 0){
                    //从epoll树上删除
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    //关闭文件描述符(避免fd泄露)
                    close(fd);
                    continue;
                }

                 // ======================
                // 业务逻辑：回显（收到什么发回去什么）
                // ======================
                write(fd, buf, n);

                 // ======================
                // 加这 2 行！压测立刻满血！
                //短连接测压
                // ======================
                
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
            }
        }
    }
    // 关闭资源（理论上服务器不会跑到这里）
     close(epoll_fd);
    close(sockfd);
    return;
}
