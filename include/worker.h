// =========================================
// Worker 工作线程类
// 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
// =========================================
#ifndef WORKER_H
#define WORKER_H

#include <thread>       // 线程库
#include <mutex>        // 互斥锁
#include <unordered_map>   // 哈希表

#include "connection.h" // Connection 结构体

class Worker{

    public:
        Worker();
        ~Worker();

        void start();   // 启动 Worker 线程
        void stop();    // 停止 Worker 线程

        //添加一个新连接到Worker的epoll
        void addConnection(int fd);
    
    private:
        void loop();    // Worker 线程的主循环
        void handleRead(Connection& conn);  // 处理读事件
        void handleWrite(Connection& conn); // 处理写事件
        void checkTimeout();    // 检查超时连接

        int epoll_fd_;      // Worker 自己的 epoll
        int notify_fd_;     // 通知 fd（eventfd）
        std::thread thread_;        // Worker 线程
        bool running_;          // 运行标志
        std::mutex mutex_;        // 互斥锁（保护 connections_）

        // 连接表（fd -> Connection）
        std::unordered_map<int, Connection> connections_;
};

#endif