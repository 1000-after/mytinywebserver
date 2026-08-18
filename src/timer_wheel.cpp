// =========================================
// 时间轮（局部版）实现
// 每个 Worker 持有独立实例，无全局锁
// 所有操作必须在 Worker::mutex_ 保护下调用
// =========================================
#include "timer_wheel.h"

// ==================== init ====================
void TimerWheel::init(int slot_count)
{
    if(slot_count < 1) slot_count = 15;
    slot_count_ = slot_count;
    current_slot_ = 0;
    slots_.resize(slot_count_);
    fd_to_slot_.clear();
}

// ==================== removeFromSlotLocked ====================
// 内部辅助：从槽位删除 fd（调用方必须已持有 Worker::mutex_）
void TimerWheel::removeFromSlotLocked(int fd)
{
    auto it = fd_to_slot_.find(fd);
    if(it == fd_to_slot_.end()) return;

    int slot_idx = it->second;
    if(slot_idx >= 0 && slot_idx < slot_count_)
    {
        slots_[slot_idx].erase(fd);
    }
    fd_to_slot_.erase(it);
}

// ==================== addConnection ====================
void TimerWheel::addConnection(int fd)
{
    // 先保险：如果 fd 已经在里面，先从旧槽位删
    removeFromSlotLocked(fd);

    // 放进当前槽位（slot_count_ 秒后指针转回这里时，就说明它超时了）
    slots_[current_slot_].insert(fd);
    fd_to_slot_[fd] = current_slot_;
}

// ==================== refreshConnection ====================
void TimerWheel::refreshConnection(int fd)
{
    // 从旧槽位删除
    removeFromSlotLocked(fd);

    // 放到当前槽位 → 重置超时计时
    slots_[current_slot_].insert(fd);
    fd_to_slot_[fd] = current_slot_;
}

// ==================== removeConnection ====================
void TimerWheel::removeConnection(int fd)
{
    removeFromSlotLocked(fd);
}

// ==================== tick ====================
// 推进指针 + 清理过期连接
// 返回：过期的 fd 列表（调用方在锁外处理关闭）
std::vector<int> TimerWheel::tick()
{
    // 指针前进一格（取模循环）
    current_slot_ = (current_slot_ + 1) % slot_count_;

    // 用 std::move 把过期 fd 的 set 拿走（O(1)）
    std::unordered_set<int> expired = std::move(slots_[current_slot_]);

    // 从反向索引里也清掉这些 fd
    for(int fd : expired)
    {
        fd_to_slot_.erase(fd);
    }

    // 转成 vector 返回，让调用方在锁外处理
    return std::vector<int>(expired.begin(), expired.end());
}
