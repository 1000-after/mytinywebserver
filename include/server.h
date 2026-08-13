// //实现基础的网络连接
// #ifndef SERVER_H
// #define SERVER_H

// //1.0
// // //服务端主逻辑（socket + bind + listen + accept）
// // #include <sys/types.h>
// // void runServer(uint16_t ports);


// // //新增设置非阻塞函数
// // int setnoblocking(int fd);

// //2.0
// #include <cstdint>  //uint16_t
// #include <sys/epoll.h>
// #include <vector>   //c++动态数组


// // =========================================
// // 每个客户端连接的数据结构（最简单，小白专用）
// // 作用：存储 fd + 读缓冲区（解决粘包/半包）
// // =========================================

// struct Connection{
//     int fd = -1;
//     std::vector<char> read_buf; // 读缓冲区
//     std::vector<char> write_buf; // 写缓冲区
//     time_t last_active_time;    //最后活跃时间(心跳/超时用)
// };

// //启动服务器
// void runServer(uint16_t ports);

// //设置非阻塞
// int setnonblocking(int fd);

// //添加fd 到epoll
// void epollAddFd(int epoll_fd, int fd);


// #endif


//6.0版本，多Reactor多线程架构
// #ifndef SERVER_H
// #define SERVER_H

// #include <cstdint>   // uint16_t

// // ==================== 通用工具函数 ====================
// // 设置非阻塞
// int setnonblocking(int fd);

// // 添加 fd 到 epoll
// void epollAddFd(int epoll_fd, int fd);

// // 启动服务器（5.0 版本）
// void runServer(uint16_t ports);

// // 启动服务器（6.0 多线程版本）
// void runServer6_0(uint16_t ports);


// // ==================== 协议相关常量 ====================
// #define MAX_EVENTS 1024 // epoll_wait 一次最多处理的事件数
// #define BUF_SIZE 1024   // 每次 read 读取的临时缓冲区大小
// #define MAX_PACKET_SIZE 65536       // 最大包长度（64KB）
// #define IDLE_TIMEOUT 15 // 超时时间（秒）
// #define CHECK_INTERVAL 3 // 超时检查间隔（秒）



// // ==================== 协议头（固定 4 字节）====================
// #pragma pack(push, 1)
// struct PacketHeader{
//     uint32_t data_len;  // 数据体长度（网络字节序）
// };
// #pragma pack(pop)

// #endif

//6.1协议换成http协议
#ifndef SERVER_H
#define SERVER_H

#include <cstdint>   // uint16_t

// ==================== 通用工具函数 ====================
// 设置非阻塞
int setnonblocking(int fd);

// 添加 fd 到 epoll
void epollAddFd(int epoll_fd, int fd);

// 启动服务器（5.0 版本）
void runServer(uint16_t ports);

// 启动服务器（6.0 多线程版本）
void runServer6_0(uint16_t ports);


// ==================== 协议相关常量（6.1版本）====================
#define MAX_EVENTS 1024     // epoll_wait 一次最多处理的事件数
#define BUF_SIZE 8192       // 每次 read 读取的缓冲区大小（8KB）
#define MAX_HTTP_HEADER 16384  // HTTP 头最大长度（16KB）
#define IDLE_TIMEOUT 15     // 超时时间（秒）
#define CHECK_INTERVAL 3    // 超时检查间隔（秒）



// ==================== HTTP 响应常量 ====================
// 响应体 "Hello, WebBench!" = 16 字节，必须和 Content-Length 精确匹配

// 长连接响应（默认，服务器端主动保持连接）
#define HTTP_RESPONSE_OK \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Length: 16\r\n" \
    "Content-Type: text/plain\r\n" \
    "Connection: keep-alive\r\n" \
    "\r\n" \
    "Hello, WebBench!"

// 🆕 短连接响应（Body 相同，只是 Connection: close，用于客户端请求 close 的情况）
#define HTTP_RESPONSE_OK_CLOSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Length: 16\r\n" \
    "Content-Type: text/plain\r\n" \
    "Connection: close\r\n" \
    "\r\n" \
    "Hello, WebBench!"

// 404 响应（"Not Found" = 9 字节，正确。此响应自动关连接）
#define HTTP_RESPONSE_404 \
    "HTTP/1.1 404 Not Found\r\n" \
    "Content-Length: 9\r\n" \
    "Content-Type: text/plain\r\n" \
    "Connection: close\r\n" \
    "\r\n" \
    "Not Found"

#endif