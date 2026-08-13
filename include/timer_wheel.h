// =========================================
// 时间轮（全局单例版）
// 作用：管理所有连接的超时，超时后回调通知外部关连接
// =========================================
#ifndef TIMER_WHEEL_H
#define TIMER_WHEEL_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

class TimerWheel{
    public:
        // ===== 回调类型：超时时通知外部（参数 = 超时的 fd）=====
        using TimeoutCallback = std::function<void(int fd)>;

        // ===== 单例模式（和 Logger 完全相同的写法）=====
        static TimerWheel& instance();

        // 禁用拷贝/赋值（单例）
        TimerWheel(const TimerWheel&) = delete;
        TimerWheel& operator=(const TimerWheel&) = delete;

        // ===== 启动/停止后台滴答线程 =====
        void init();
        void shutdown();

        // ===== 3 个核心对外操作（线程安全，内部加锁）=====
        void addConnection(int fd); // 新连接加入 → 放进当前槽位
        void refreshConnection(int fd); // 连接有活动 → 从旧槽位移到当前槽位（刷新计时）
        void removeConnection(int fd);      // 连接主动关闭 → 从时间轮里删除

        // ===== 注册回调：超时时会被调用（锁释放后才调，防死锁）=====
        void setCallback(TimeoutCallback cb);
    
    private:
        // ===== 构造/析构（私有，单例）=====
        TimerWheel();
        ~TimerWheel();

        // ===== 常量 =====
        static const int SLOT_COUNT = 15;   // 槽位数 = 超时秒数（15 秒）


        // ===== 数据结构 =====
        // 槽位数组：每个槽位存一组 fd（用 unordered_set，插入/删除/查找 O(1)）
        std::vector<std::unordered_set<int>> slots_;
        // 当前指针（0~SLOT_COUNT-1 循环）
        int current_slot_;
        // 反向索引：fd → 当前在哪个槽位（让 refresh/remove 不用遍历 15 个槽）
        std::unordered_map<int, int> fd_to_slot_;

        // ===== 线程同步 =====
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::thread tick_thread_;
        std::atomic<bool> running_{false};

        // 回调（超时时调用，在锁外执行）
        TimeoutCallback callback_;

        // ===== 内部方法 =====
        void tickThread();  // 后台滴答线程主循环
        void removeFromSlotLocked(int fd);  // 从槽位删除 fd（调用方必须已持有 mutex_ 锁！）
        
};

#endif  // TIMER_WHEEL_H