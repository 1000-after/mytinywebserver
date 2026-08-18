// // =========================================
// // 线程池实现
// // 管理多个 Worker 线程，负责将新连接分发给 Worker
// // 调度策略：Round-Robin（轮询）
// // =========================================
// #include <stdio.h>  // printf
// #include <stdlib.h>     // exit

// #include "threadpool.h"     // ThreadPool 类声明

// // ==================== 构造函数 ====================
// // 参数 num_threads: Worker 线程数量，默认 4
// ThreadPool::ThreadPool(int num_threads)
//     :worker_count_(num_threads),    // 保存线程数量
//     current_index_(0)   // 初始轮询索引为 0
// {
//     // 根据指定数量创建 Worker 对象
//     for(int i =0; i < worker_count_; ++i){
//         Worker* worker = new Worker();      // 创建 Worker 对象
//         workers_.push_back(worker);     // 加入 Worker 数组
//     }

//     printf("线程池已创建，共 %d 个 Worker\n", worker_count_);
// }

// // ==================== 析构函数 ====================
// ThreadPool::~ThreadPool()
// {
//     // 先停止所有 Worker（确保安全退出）
//     stop();

//     //释放Worker对象内存
//     for(Worker* worker : workers_)
//     {
//         delete worker;  // 调用 Worker 的析构函数
//     }

//     workers_.clear();   // 清空数组

//     printf("线程池已销毁\n");
// }

// // ==================== 启动所有 Worker ====================
// void ThreadPool::start()
// {
//     // 遍历所有 Worker，逐个启动
//     for(int i = 0; i < worker_count_; ++i)
//     {
//         workers_[i]->start();     // 启动第 i 个 Worker
//         printf("启动 Worker[%d] 完成\n", i);
//     }

//     printf("线程池已启动，%d 个 Worker 线程运行中\n", worker_count_);

// }


// // ==================== 停止所有 Worker ====================
// void ThreadPool::stop()
// {
//     // 遍历所有 Worker，逐个停止
//     for(int i =0; i < worker_count_; ++i)
//     {
//         workers_[i]->stop();    // 停止第 i 个 Worker
//         printf("停止 Worker[%d] 完成\n", i);
//     }

//     printf("线程池已停止\n");
// }

// // ==================== 分发明连接 ====================
// // 使用 Round-Robin（轮询）策略将连接分配给 Worker
// // 优势：简单、公平
// // 劣势：不考虑 Worker 当前负载
// void ThreadPool::distributeConnection(int fd){
//     // 加锁保护 current_index_
//     // 防止多线程竞争导致索引错误
//     std::lock_guard<std::mutex> lock(mutex_);

//     // 计算目标 Worker 索引（轮询）
//     // 使用取模运算确保索引在有效范围内
//     int target_index = current_index_ % worker_count_;

//     // 更新索引（下次用下一个 Worker）
//     current_index_++;

//     // 获取目标 Worker 指针
//     Worker* target_worker = workers_[target_index];

//     // 将连接分配给目标 Worker
//     // 调用 Worker 的 addConnection 方法
//     target_worker->addConnection(fd);

//     printf("连接 fd=%d 分配给 Worker[%d]\n", fd, target_index);
// }

// // ==================== 示例：Round-Robin 工作原理 ====================
// //假设有 3 个 Worker：W[0], W[1], W[2]
// //
// // 第 1 个连接 -> W[0] (current_index_=0)
// // 第 2 个连接 -> W[1] (current_index_=1)
// // 第 3 个连接 -> W[2] (current_index_=2)
// // 第 4 个连接 -> W[0] (current_index_=3, 3%3=0)
// // 第 5 个连接 -> W[1] (current_index_=4, 4%3=1)
// // ... 依次循环
// // =========================================================



// =========================================
// 线程池实现
// 管理多个 Worker 线程，负责将新连接分发给 Worker
// 调度策略：Round-Robin（轮询）
//7.0添加日志
// =========================================
#include <stdio.h>
#include <stdlib.h>

#include "threadpool.h"
#include "logger.h"  // 🆕 引入日志系统

// ==================== 构造函数 ====================
// 参数 num_threads: Worker 线程数量，默认 4
ThreadPool::ThreadPool(int num_threads)
    :worker_count_(num_threads),    // 保存线程数量
    current_index_(0)   // 初始轮询索引为 0
{
    // 根据指定数量创建 Worker 对象
    for(int i =0; i < worker_count_; ++i){
        Worker* worker = new Worker();      // 创建 Worker 对象
        workers_.push_back(worker);     // 加入 Worker 数组
    }

    LOG_INFO("线程池已创建，共 %d 个 Worker", worker_count_);
}

// ==================== 析构函数 ====================
ThreadPool::~ThreadPool()
{
    // 先停止所有 Worker（确保安全退出）
    stop();

    //释放Worker对象内存
    for(Worker* worker : workers_)
    {
        delete worker;  // 调用 Worker 的析构函数
    }

    workers_.clear();   // 清空数组

    LOG_INFO("线程池已销毁");
}

// ==================== 启动所有 Worker ====================
void ThreadPool::start()
{
    // 遍历所有 Worker，逐个启动
    for(int i = 0; i < worker_count_; ++i)
    {
        workers_[i]->start();     // 启动第 i 个 Worker
        LOG_INFO("启动 Worker[%d] 完成", i);
    }

    LOG_INFO("线程池已启动，%d 个 Worker 线程运行中", worker_count_);

}


// ==================== 停止所有 Worker ====================
void ThreadPool::stop()
{
    // 遍历所有 Worker，逐个停止
    for(int i =0; i < worker_count_; ++i)
    {
        workers_[i]->stop();    // 停止第 i 个 Worker
        LOG_INFO("停止 Worker[%d] 完成", i);
    }

    LOG_INFO("线程池已停止");
}

// ==================== 分发明连接 ====================
// 使用 Round-Robin（轮询）策略将连接分配给 Worker
// 优势：简单、公平
// 劣势：不考虑 Worker 当前负载
void ThreadPool::distributeConnection(int fd){
    // 加锁保护 current_index_
    // 防止多线程竞争导致索引错误
    std::lock_guard<std::mutex> lock(mutex_);

    // 计算目标 Worker 索引（轮询）
    // 使用取模运算确保索引在有效范围内
    int target_index = current_index_ % worker_count_;

    // 更新索引（下次用下一个 Worker）
    current_index_++;

    // 获取目标 Worker 指针
    Worker* target_worker = workers_[target_index];

    // 将连接分配给目标 Worker
    // 调用 Worker 的 addConnection 方法
    target_worker->addConnection(fd);

    LOG_DEBUG("连接 fd=%d 分配给 Worker[%d]", fd, target_index);
}


// ==================== 示例：Round-Robin 工作原理 ====================
//假设有 3 个 Worker：W[0], W[1], W[2]
//
// 第 1 个连接 -> W[0] (current_index_=0)
// 第 2 个连接 -> W[1] (current_index_=1)
// 第 3 个连接 -> W[2] (current_index_=2)
// 第 4 个连接 -> W[0] (current_index_=3, 3%3=0)
// 第 5 个连接 -> W[1] (current_index_=4, 4%3=1)
// ... 依次循环
// =========================================================
