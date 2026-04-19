//实现基础的网络连接
#ifndef SERVER_H
#define SERVER_H

//1.0
// //服务端主逻辑（socket + bind + listen + accept）
// #include <sys/types.h>
// void runServer(uint16_t ports);


// //新增设置非阻塞函数
// int setnoblocking(int fd);

//2.0
#include <cstdint>  //uint16_t
#include <sys/epoll.h>
#include <vector>   //c++动态数组


// =========================================
// 每个客户端连接的数据结构（最简单，小白专用）
// 作用：存储 fd + 读缓冲区（解决粘包/半包）
// =========================================

struct Connection{
    int fd = -1;
    std::vector<char> read_buf;
    std::vector<char> write_buf;
};

//启动服务器
void runServer(uint16_t ports);

//设置非阻塞
int setnonblocking(int fd);

//添加fd 到epoll
void epollAddFd(int epoll_fd, int fd);


#endif