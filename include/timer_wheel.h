// =========================================
// 时间轮（局部版）
// 每个 Worker 持有独立实例，无全局锁竞争
// 外部必须在 Worker::mutex_ 保护下操作（addConnection 由主线程调，
// refreshConnection/removeConnection/tick 由 Worker 线程调，都已经在锁内）
// tick() 返回过期 fd 列表，由调用方在锁外执行回调（防死锁）
// =========================================
#ifndef TIMER_WHEEL_H
#define TIMER_WHEEL_H

#include <vector>
#include <unordered_set>
#include <unordered_map>

class TimerWheel{
    public:
        TimerWheel() = default;
        ~TimerWheel() = default;

        // 禁用拷贝/赋值（每个 Worker 独占一个时间轮）
        TimerWheel(const TimerWheel&) = delete;
        TimerWheel& operator=(const TimerWheel&) = delete;

        // 初始化：分配 slot_count 个槽位（slot_count = 超时秒数）
        void init(int slot_count = 15);

        // 添加连接到时间轮（放进当前槽位）
        void addConnection(int fd);

        // 刷新连接（从旧槽位移到当前槽位 → 重置超时计时）
        void refreshConnection(int fd);

        // 主动移除连接
        void removeConnection(int fd);

        // 推进指针 + 清理过期连接
        // 返回：过期的 fd 列表（调用方在锁外处理这些 fd 的关闭）
        std::vector<int> tick();

        // 当前指针位置（调试用）
        int currentSlot() const { return current_slot_; }

    private:
        int slot_count_ = 15;               // 槽位数 = 超时秒数
        int current_slot_ = 0;              // 当前指针（0 ~ slot_count_-1 循环）

        std::vector<std::unordered_set<int>> slots_;  // 槽位数组
        std::unordered_map<int, int> fd_to_slot_;     // 反向索引：fd → 槽位

        // 内部：从槽位删除 fd（调用方必须已持有 Worker::mutex_）
        void removeFromSlotLocked(int fd);
};

#endif  // TIMER_WHEEL_H
