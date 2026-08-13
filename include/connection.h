// // =========================================
// // 连接结构体定义
// // 作用：存储每个客户端连接的所有信息
// // =========================================
// #ifndef CONNECTION_H
// #define CONNECTION_H

// #include <vector>   //动态数组
// #include <time.h>   //时间函数


// struct Connection{
//     int fd = -1;        // 客户端文件描述符
//     std::vector<char> read_buf;         // 读缓冲区（解决粘包/半包）
//     std::vector<char> write_buf;        // 写缓冲区（异步发送）
//     time_t last_active_time;        // 最后活跃时间（心跳/超时用）
// };

// #endif


// =========================================
// 连接结构体定义
// 6.1版本：支持 HTTP 协议
// =========================================
#ifndef CONNECTION_H
#define CONNECTION_H

#include <vector>   //动态数组
#include <time.h>   //时间函数
#include <string>   // 字符串


struct Connection{
    int fd = -1;        // 客户端文件描述符
    std::vector<char> read_buf;         // 读缓冲区（解决粘包/半包）
    std::vector<char> write_buf;        // 写缓冲区（异步发送）
    time_t last_active_time;        // 最后活跃时间（心跳/超时用）

    // HTTP 解析状态
    bool http_parsed;               // HTTP 请求是否已解析
    std::string http_method;        // HTTP 方法（GET/POST）
    std::string http_path;          // 请求路径
    bool http_keep_alive;           // 是否保持连接

    // 构造函数
    Connection() : fd(-1), last_active_time(0),
                   http_parsed(false), http_keep_alive(true) {}
                   
};

#endif