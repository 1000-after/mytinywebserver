// =========================================
// ConnectionPool：连接池（9.0 新增）
// 作用：预分配一批 Connection 对象壳子（含读/写缓冲区预扩容），
//       新连接时 acquire 借出，关闭时 release 归还（不析构不释放），
//       消灭高并发下频繁 malloc/free 的锁竞争和外部碎片。
// 设计：
//   - 每个 Worker 持有自己独立的连接池（分片），几乎无跨线程锁竞争
//   - 空闲队列使用 std::queue<Connection*>（FIFO，也可改 LIFO stack）
//   - 池子空时：动态 new 新对象（策略A：自动扩容）
//   - 归还时：只清脏数据，保留 vector capacity，下次借出直接复用
// =========================================
#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <queue>      // 空闲对象队列（先进先出）
#include <mutex>      // 互斥锁（虽然分片后几乎只被本Worker碰，但 addConnection 在主线程调用，仍要锁）
#include <cstddef>    // size_t

#include "connection.h"   // Connection 结构体

class ConnectionPool {
public:
    // ==================== 构造 / 析构 ====================

    // 构造函数
    //   init_count : 启动时预分配的对象数量
    //   reserve_buf_bytes : 每个对象的 read_buf / write_buf 预扩容字节数
    //   max_count  : 池子最大对象数（超过则不再入队，直接 delete 防内存泄漏）
    ConnectionPool(size_t init_count = 64,
                   size_t reserve_buf_bytes = 8192,
                   size_t max_count = 2048);

    // 析构函数：把空闲队列里所有对象真正 delete 掉（归还内存给 OS）
    ~ConnectionPool();

    // ==================== 核心接口 ====================

    // 借出一个 Connection 对象（新连接创建时调用）
    //   - 队列非空：从队头取一个预先分配好的返回
    //   - 队列空：new 一个新对象返回（自动扩容策略）
    // 返回值：非空 Connection*，调用方负责在用完后调 release() 归还
    Connection* acquire();

    // 归还一个 Connection 对象（连接关闭时调用）
    //   - 不 delete，只做"软清理"（清 fd、清 buffer 内容但保留 capacity）
    //   - 清理完后塞回空闲队尾，留着下次 acquire
    //   - 若当前池子已超过 max_count：直接 delete（防止无限膨胀）
    // 参数 c：必须是之前 acquire() 返回的指针，不能为 nullptr
    void release(Connection* c);

    // ==================== 统计接口（调试用） ====================
    size_t freeCount()  const { return free_queue_.size(); }   // 当前空闲对象数
    size_t totalCount() const { return total_allocated_; }     // 累计分配过的对象总数

private:
    // ==================== 内部工具函数 ====================

    // 软清理：把对象内容重置成"刚构造好的状态"，但不释放底层内存
    // （保留 vector 的 capacity，这是连接池消灭 malloc 的关键）
    void softReset(Connection* c);

    // ==================== 成员变量 ====================
    std::queue<Connection*> free_queue_;   // 空闲对象队列（存指针，避免对象拷贝）
    mutable std::mutex      mutex_;        // 保护 free_queue_（主线程 acquire、工作线程 release 可能并发）
    size_t                  reserve_buf_;  // read_buf / write_buf 的预扩容字节数
    size_t                  max_count_;    // 池子最大容量（防内存泄漏的安全闸）
    size_t                  total_allocated_; // 累计分配的对象数（用于统计/上限）
};

#endif // CONNECTION_POOL_H