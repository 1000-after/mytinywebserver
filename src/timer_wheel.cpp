// =========================================
// 时间轮（全局单例版）
// =========================================
#include "timer_wheel.h"
#include "logger.h"

// ==================== 单例实例 ====================
TimerWheel& TimerWheel::instance()
{
    static TimerWheel instance;
    return instance;
}

// ==================== 构造/析构 ====================
TimerWheel::TimerWheel(): current_slot_(0)
{
    slots_.resize(SLOT_COUNT);
}

TimerWheel::~TimerWheel()
{
    shutdown();
}

// ==================== init / shutdown ====================
void TimerWheel::init()
{
    if(running_.load()) return; // 已经初始化过就直接返回（幂等）

    running_ = true;
    tick_thread_ = std::thread(&TimerWheel::tickThread, this);
    LOG_INFO("TimerWheel: 初始化完成，%d 个槽位（%d 秒超时），后台滴答线程已启动",
            SLOT_COUNT, SLOT_COUNT);
}

void TimerWheel::shutdown()
{
    bool expected = true;
    if(!running_.compare_exchange_strong(expected, false))
    {
        // 已经是 false（没启动或已经 shutdown 过），直接返回
        return;
    }

    cv_.notify_all();       // 唤醒可能在 wait_for 里的滴答线程，立刻退出
    if(tick_thread_.joinable())
    {
        tick_thread_.join();
    }

    // 清理资源（防止万一再用）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& slot : slots_) slot.clear();
        fd_to_slot_.clear();
    }
    callback_ = nullptr;
    LOG_INFO("TimerWheel: 已停止，后台线程已退出");

}

// ==================== 回调注册 ====================
void TimerWheel::setCallback(TimeoutCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

// ==================== 内部辅助：从槽位删除 fd（调用方必须已拿锁！）====================
void TimerWheel::removeFromSlotLocked(int fd)
{
    auto it = fd_to_slot_.find(fd);
    if(it == fd_to_slot_.end()) return;     // 找不到就跳过（可能超时后已经被 tick 清过了）

    int slot_idx = it->second;
    if(slot_idx >= 0 && slot_idx < SLOT_COUNT)
    {
        slots_[slot_idx].erase(fd);     // 从槽位的 set 里删掉
    }
    fd_to_slot_.erase(it);      // 从反向索引删掉
}

// ==================== 3 个核心操作（线程安全）====================
void TimerWheel::addConnection(int fd){
    std::lock_guard<std::mutex> lock(mutex_);

    //先保险:如果fd已经在里面(理论上不应该),先从旧槽位删
    removeFromSlotLocked(fd);

    // 放进当前槽位（15 秒后指针转回这里时，就说明它 15 秒没活动了）
    slots_[current_slot_].insert(fd);
    fd_to_slot_[fd] = current_slot_;

    LOG_DEBUG("TimerWheel: fd=%d 加入时间轮，槽位=%d", fd, current_slot_);
}

void TimerWheel::refreshConnection(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 从旧槽位删除
    removeFromSlotLocked(fd);

    // 放到当前槽位 → 相当于"重置计时器"，15 秒从现在开始再算
    slots_[current_slot_].insert(fd);
    fd_to_slot_[fd] = current_slot_;

    LOG_DEBUG("TimerWheel: fd=%d 刷新计时，新槽位=%d", fd, current_slot_);
}


void TimerWheel::removeConnection(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    removeFromSlotLocked(fd);
    LOG_DEBUG("TimerWheel: fd=%d 主动从时间轮删除", fd);
}

// ==================== 后台滴答线程主循环（核心）====================
void TimerWheel::tickThread()
{
    LOG_INFO("TimerWheel: 滴答线程开始运行");

    while(running_.load())
    {
        // ┌──────────────────────────────────────────────────┐
        // │ 要点 1：用 cv_.wait_for 而非 sleep_for            │
        // │   - 每 1 秒自动醒一次（推进指针）                  │
        // │   - shutdown 时 running_=false + notify_all()     │
        // │     → 立刻被唤醒，不用傻等 1 秒                    │
        // │   - lambda 谓词 = "running_ 变 false 就别等了"     │
        // └──────────────────────────────────────────────────┘
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(1), [this]{
            return !running_.load();
        });

        if(!running_.load()){
            break;      // shutdown 了，直接退出线程
        }

        // ┌──────────────────────────────────────────────────┐
        // │ 要点 2：指针前进一格（取模 15 循环）               │
        // └──────────────────────────────────────────────────┘
        current_slot_ = (current_slot_ + 1) % SLOT_COUNT;

        // ┌──────────────────────────────────────────────────┐
        // │ 要点 3：用 std::move 把过期 fd 的 set 拿走         │
        // │   - move 只换内部指针，O(1)，和 swap 等价          │
        // │   - 拿走后 slots_[current_slot_] 变成合法空状态    │
        // │   - 同步清理反向索引（这些 fd 不用再 refresh 了）   │
        // └──────────────────────────────────────────────────┘
        std::unordered_set<int> expired = std::move(slots_[current_slot_]);

        // 从反向索引里也清掉（tick 处理过后，这些 fd 就不在时间轮里了）
        for(int fd : expired)
        {
            fd_to_slot_.erase(fd);
        }

        // 把回调也拷贝一份（回调调用期间不拿锁，防止 callback 里改 callback_）
        TimeoutCallback cb = callback_;

        // ┌──────────────────────────────────────────────────┐
        // │ 要点 4：调用回调前**必须释放锁**！                  │
        // │   - 回调里会去 Worker 关连接 → Worker 会拿自己锁    │
        // │   - Worker 那边拿自己锁时可能调 refreshConnection  │
        // │     → 要拿时间轮锁 → 形成"时间轮锁↔Worker锁"死锁  │
        // │   - 先 unlock，再 cb(fd)，彻底规避这个问题         │
        // └──────────────────────────────────────────────────┘
        lock.unlock();

        // 无锁状态下调用回调
        int timeout_count = 0;
        if(cb)
        {
            for(int fd : expired)
            {
                LOG_DEBUG("TimerWheel: fd=%d 超时（%d 秒无活动）", fd, SLOT_COUNT);
                cb(fd);
                ++timeout_count;
            }
        }

        if(timeout_count > 0)
        {
            LOG_INFO("TimerWheel: 指针=%d，本轮清理超时连接 %d 个",
                    current_slot_, timeout_count);
        }
    }
    LOG_INFO("TimerWheel: 滴答线程已退出");
}


