// =========================================
// 连接结构体定义
// 作用：存储每个客户端连接的所有信息
// =========================================
#ifndef CONNECTION_H
#define CONNECTION_H

#include <vector>   //动态数组
#include <time.h>   //时间函数

struct Connection{
    int fd = -1;        // 客户端文件描述符
    std::vector<char> read_buf;         // 读缓冲区（解决粘包/半包）
    std::vector<char> write_buf;        // 写缓冲区（异步发送）
    time_t last_active_time;        // 最后活跃时间（心跳/超时用）
};

#endif