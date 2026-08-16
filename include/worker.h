// =========================================
// Worker 工作线程类
// 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
// =========================================
#ifndef WORKER_H
#define WORKER_H

#include <thread>       // 线程库
#include <mutex>        // 互斥锁
#include <unordered_map>   // 哈希表

#include "connection.h"       // Connection 结构体
#include "connection_pool.h"  // 🆕 9.0：连接池（预分配 Connection 对象）

class Worker{

    public:
        Worker();
        ~Worker();

        void start();   // 启动 Worker 线程
        void stop();    // 停止 Worker 线程

        //添加一个新连接到Worker的epoll
        void addConnection(int fd);

        // 🆕 给 TimerWheel 回调调用：
        //   如果 fd 属于这个 Worker → 安全关闭它并返回 true
        //   否则 → 返回 false
        bool tryCloseConnection(int fd);
    
    private:
        void loop();    // Worker 线程的主循环
        // 🟢 handleRead/Write 不再直接 erase/close fd，只用引用返回 3 个状态：
        //   need_close       = 子函数认为这个连接该关（erase + EPOLL_CTL_DEL）
        //   close_after_unlock = 需要在锁释放后再 close(fd)（避免死锁/重复关）
        //   need_erase_only  = 已 erase，但外面不需要再 EPOLL_CTL_DEL/close（完全交给 loop）
        void handleRead(Connection& conn, bool& need_close, bool& close_after_unlock);
        void handleWrite(Connection& conn, bool& need_close, bool& close_after_unlock);
        void checkTimeout();    // 检查超时连接

        int epoll_fd_;      // Worker 自己的 epoll
        int notify_fd_;     // 通知 fd（eventfd）
        std::thread thread_;        // Worker 线程
        bool running_;          // 运行标志
        std::mutex mutex_;        // 互斥锁（保护 connections_）

        // 连接池（9.0 新增）
        // 每个 Worker 独立持有自己的池子（分片），减少跨线程锁竞争
        // 指针而非对象：因为构造函数里要先拿配置参数（pool_init_count 等）再 init，
        // 所以延迟到 Worker::start() 里 new
        ConnectionPool* conn_pool_;

        // 连接表（fd -> Connection*）
        // 🆕 9.0 改造：value 从 Connection 对象改成指针
        //   - 指针来自 conn_pool_->acquire()，用完归还 conn_pool_->release()
        //   - unordered_map 自己不再管理 Connection 的构造/析构，交给池子
        std::unordered_map<int, Connection*> connections_;
};

#endif