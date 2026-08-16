// =========================================
// ConnectionPool 实现（9.0 新增）
// =========================================
#include <cstring>    // memset

#include "connection_pool.h"
#include "logger.h"   // LOG_INFO / LOG_WARN

// ==================== 构造函数 ====================
// 步骤：
//   1. 记录预扩容大小和最大容量
//   2. 循环 new init_count 个 Connection 对象
//   3. 对每个对象：reserve 读/写缓冲区 + HTTP 字符串
//   4. 全部塞进空闲队列
ConnectionPool::ConnectionPool(size_t init_count,
                               size_t reserve_buf_bytes,
                               size_t max_count)
    : reserve_buf_(reserve_buf_bytes),
      max_count_(max_count),
      total_allocated_(0)
{
    LOG_INFO("ConnectionPool: 开始预分配 %zu 个 Connection，每个 buf 预扩容 %zu 字节",
             init_count, reserve_buf_bytes);

    for (size_t i = 0; i < init_count; ++i) {
        // ① 在堆上 new 一个 Connection（调用默认构造函数）
        Connection* c = new Connection();

        // ② 预分配读缓冲区 / 写缓冲区 capacity
        //    reserve 只改变 capacity 不改变 size，
        //    后续 handleRead/Write 里 resize / insert 时不会再触发 realloc
        c->read_buf.reserve(reserve_buf_bytes);
        c->write_buf.reserve(reserve_buf_bytes);

        // ③ 预分配 HTTP 字段的 capacity（避免 SSO 外的小内存反复分配）
        c->http_method.reserve(16);    // "GET"/"POST" 最长也就 8 字节，留点余量
        c->http_path.reserve(512);     // URL 路径一般不超过 256

        // ④ 放入空闲队列
        free_queue_.push(c);

        // ⑤ 计数
        ++total_allocated_;
    }

    LOG_INFO("ConnectionPool: 预分配完成，空闲对象数 = %zu，累计 = %zu",
             free_queue_.size(), total_allocated_);
}

// ==================== 析构函数 ====================
// 把空闲队列里的所有对象真正 delete 掉
// 注意：析构不负责"借出中但没归还的对象"——调用方（Worker 析构）必须保证在析构池前
//       把 connections_ 里所有在用的连接先 release 回来，或者自己 delete
ConnectionPool::~ConnectionPool()
{
    // 加锁（虽然析构时通常已经单线程了，但保险起见）
    std::lock_guard<std::mutex> lock(mutex_);

    size_t leaked = 0;   // 统计丢失的对象（应该是 0）

    while (!free_queue_.empty()) {
        Connection* c = free_queue_.front();
        free_queue_.pop();
        delete c;   // 真正释放内存（vector 析构 + Connection 析构）
        --total_allocated_;
    }

    // 如果 total_allocated_ 还没归零 → 说明有对象借出后没归还（泄漏）
    if (total_allocated_ > 0) {
        leaked = total_allocated_;
    }

    LOG_INFO("ConnectionPool: 析构完成，泄漏对象数 = %zu（应=0）", leaked);
}

// ==================== acquire：借出对象 ====================
Connection* ConnectionPool::acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!free_queue_.empty()) {
        // -------- 队列里有空闲对象 → 直接取队头 --------
        Connection* c = free_queue_.front();
        free_queue_.pop();

        // 🔍 安全性检查：fd 应该已经被重置为 -1 了（release 会清）
        //    如果不是，说明有 bug：归还时没清，或者被外部篡改
        if (c->fd != -1) {
            LOG_WARN("ConnectionPool::acquire 警告：借出的对象 fd=%d（应=-1），强制重置", c->fd);
            c->fd = -1;
        }

        return c;
    }

    // -------- 队列为空 → 动态 new 一个（自动扩容策略A） --------
    Connection* c = new Connection();
    c->read_buf.reserve(reserve_buf_);
    c->write_buf.reserve(reserve_buf_);
    c->http_method.reserve(16);
    c->http_path.reserve(512);

    ++total_allocated_;

    LOG_WARN("ConnectionPool: 池子空了，动态扩容 1 个，累计分配 = %zu", total_allocated_);
    return c;
}

// ==================== release：归还对象 ====================
void ConnectionPool::release(Connection* c)
{
    if (c == nullptr) {
        LOG_WARN("ConnectionPool::release 收到 nullptr，忽略");
        return;
    }

    // ① 先做软清理（清内容，保留容量）
    softReset(c);

    std::lock_guard<std::mutex> lock(mutex_);

    // ② 判断池子是否超过最大上限
    if (total_allocated_ > max_count_ && free_queue_.size() >= max_count_) {
        // 超过上限：直接 delete 这个对象，不塞回队列（防止池子无限膨胀）
        delete c;
        --total_allocated_;
        LOG_WARN("ConnectionPool: 超过 max_count=%zu，归还时直接 delete，累计 = %zu",
                 max_count_, total_allocated_);
        return;
    }

    // ③ 未超上限：塞回队尾，下次 acquire 直接用
    free_queue_.push(c);
}

// ==================== softReset：软清理（不释放内存，只清数据） ====================
// 关键思想：
//   - vector 用 resize(0) 或 memset+resize(0)：size 变 0，但 capacity 保持不变
//   - string 用 clear()：通常也保留 capacity（SSO 字符串本来就不占堆）
//   - 这样下次借出时，读/写几百字节都不会触发 realloc
void ConnectionPool::softReset(Connection* c)
{
    // ---- 基本字段清零 ----
    c->fd = -1;                       // 重要：标记为"未绑定 fd"的空闲状态
    c->last_active_time = 0;

    // ---- HTTP 解析状态重置 ----
    c->http_parsed = false;
    c->http_keep_alive = true;        // 默认 keep-alive，和构造函数一致

    // ---- read_buf：清零数据但保留 capacity ----
    if (!c->read_buf.empty()) {
        // 用 memset 刷掉旧请求残留（安全层面：避免下一个连接读到上个请求的敏感数据）
        // 注意：data() 返回底层数组首地址，size() 是当前有效长度
        std::memset(c->read_buf.data(), 0, c->read_buf.size());
    }
    c->read_buf.resize(0);   // size=0，但 reserve 过的 capacity 仍=8192（或更大）

    // ---- write_buf：同上 ----
    if (!c->write_buf.empty()) {
        std::memset(c->write_buf.data(), 0, c->write_buf.size());
    }
    c->write_buf.resize(0);

    // ---- HTTP 字符串：清空内容 ----
    c->http_method.clear();
    c->http_path.clear();
}