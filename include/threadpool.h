// =========================================
// 线程池类
// 管理多个 Worker 线程，负责将新连接分发给 Worker
// =========================================
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>   // 动态数组
#include <mutex>        // 互斥锁
#include "worker.h"     // Worker 类
class ThreadPool{
    public:
        ThreadPool(int num_threads = 4);    //析构函数，指定线程数
        ~ThreadPool();          // 析构函数

        void start();            // 启动所有 Worker
        void stop();        // 停止所有 Worker

        // 分发明连接到某个 Worker（Round-Robin 策略）
        void distributeConnection(int fd);

        // 🆕 给 TimerWheel 回调调用：
        //   遍历所有 Worker，让持有 fd 的那个 Worker 关闭连接
        //   找到并关闭返回 true，找不到返回 false
        bool tryCloseConnectionOnAnyWorker(int fd);

    private:
        std::vector<Worker*> workers_;      // Worker 数组
        int worker_count_;      // Worker 数量
        int current_index_;     // 当前轮询索引
        std::mutex mutex_;      // 互斥锁（保护 current_index_）
};

#endif