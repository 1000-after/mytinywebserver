// =========================================
// 8.0 版本：信号处理实现
// =========================================
#include "signal_handler.h"  // 自己的头文件
#include <pthread.h>          // pthread_sigmask()：设置/获取线程的信号阻塞掩码

// ==================== 静态成员变量定义（必须写在 .cpp 里！）====================
// 类里只是声明了 static atomic，这里才是真正的「定义+初始化」
// false 表示初始状态：没有请求关闭
std::atomic<bool> SignalHandler::g_shutdown_flag_{false};

// ==================== 信号回调：收到信号后内核自动调这个函数 ====================
// ⚠️ 异步信号安全规则：
//    信号处理函数执行时，程序可能正卡在 malloc/printf 的中间，
//    所以这里绝对不能调任何非异步信号安全的函数！
//    我们只做一件事：写 atomic 变量，这是 CPU 单指令，绝对安全
void SignalHandler::onSignal(int /*signum*/) {
    // signum 参数没用到（SIGINT 和 SIGTERM 我们都做同一件事），所以注释掉参数名避免编译警告
    // store() = 原子写入，memory_order_release 保证写之前的所有内存操作都不会被重排到这句之后
    g_shutdown_flag_.store(true, std::memory_order_release);
}

// ==================== 初始化信号处理器 ====================
// 必须在 main 函数最开头调用（在创建线程/开监听之前！）
void SignalHandler::init() {
    // ---------- 第1步：忽略 SIGPIPE ----------
    // 场景：客户端调用 close() 后，服务器还调用 write() 往这个 fd 写数据
    //       内核会给服务器进程发 SIGPIPE 信号，默认动作是【直接终止进程】
    //       这对高并发服务器是灾难：一个客户端断开 = 整个服务器崩了
    // 解决：SIG_IGN（忽略），这样 write() 会正常返回 -1 且 errno=EPIPE，我们自己处理就行
    struct sigaction sa_ign = {};          // 清零初始化（C++11 花括号初始化）
    sa_ign.sa_handler = SIG_IGN;           // 处理方式 = 忽略
    sigemptyset(&sa_ign.sa_mask);          // 不阻塞其他信号
    sa_ign.sa_flags = 0;                   // 无特殊标志
    // 注册 SIGPIPE 的处理方式
    // 参数1 = 目标信号，参数2 = 新动作，参数3 = 旧动作（不需要则 nullptr）
    sigaction(SIGPIPE, &sa_ign, nullptr);

    // ---------- 第2步：捕获 SIGINT + SIGTERM ----------
    // SIGINT  = 用户按 Ctrl+C 时发送
    // SIGTERM = kill 进程时默认发送（kill -15 / kill 默认）
    // 注意：kill -9 发的是 SIGKILL，这个信号【无法捕获也无法忽略】，所以不用管它
    struct sigaction sa_exit = {};
    sa_exit.sa_handler = &SignalHandler::onSignal;   // 自定义回调函数
    sigemptyset(&sa_exit.sa_mask);                   // 回调执行期间不阻塞其他信号

    // SA_RESTART 标志（重要！）：
    //   如果信号触发时程序正阻塞在 epoll_wait / accept / read 这类慢速系统调用里，
    //   没有 SA_RESTART → 系统调用返回 -1 且 errno=EINTR，我们要自己处理重试
    //   有 SA_RESTART    → 内核自动重启被打断的系统调用，不用我们管，代码更简单
    //   但 8.0 为了让主循环能及时检测 g_shutdown_flag_，我们故意不设这个标志，
    //   这样 epoll_wait 被信号打断后会立刻返回，马上检测到标志位就退出
    sa_exit.sa_flags = 0;

    // 同时注册 SIGINT 和 SIGTERM，两个信号共用同一个处理函数
    sigaction(SIGINT,  &sa_exit, nullptr);
    sigaction(SIGTERM, &sa_exit, nullptr);

    // ---------- 第3步：【关键】阻塞 SIGINT + SIGTERM ----------
    // 为什么要阻塞？
    //   在多线程程序中，信号可以被内核投递给【任意一个】未阻塞该信号的线程
    //   如果 Worker 线程收到了 SIGINT：
    //     → Worker 的 onSignal() 会设置 g_shutdown_flag_ = true
    //     → 但主线程的 epoll_wait()【不会被打断】（信号投递到了别的线程）
    //     → 主线程的 epoll_wait 继续阻塞，可能等 500ms 超时才检查标志
    //     → 更糟的是：如果 listen fd 一直有事件（压测中），epoll_wait 永远不会超时！
    //   解决方案：在 init() 中阻塞 SIGINT/SIGTERM
    //     → 之后创建的 Worker 线程【继承已阻塞的信号掩码】，无法接收这些信号
    //     → 主线程在 allowSignals() 中解除阻塞后，成为唯一能接收信号的线程
    //     → Ctrl+C 立刻打断主线程的 epoll_wait，触发优雅关闭
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    // SIG_BLOCK = 把 set 中的信号加入当前线程的阻塞掩码
    // 由于 Worker 线程在 init() 之后创建，它们会继承这个阻塞掩码
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

// ==================== 解除主线程的信号阻塞 ====================
// 必须在进入 epoll_wait 事件循环前调用！
// init() 中阻塞了 SIGINT/SIGTERM（Worker 线程继承了阻塞，收不到信号）
// 这里解除主线程的阻塞，让主线程成为唯一能接收退出信号的线程
// 这样 Ctrl+C/kill 会立刻打断主线程的 epoll_wait()，触发优雅关闭
void SignalHandler::allowSignals() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    // SIG_UNBLOCK = 把 set 中的信号从当前线程的阻塞掩码中移除
    // 现在只有主线程能接收 SIGINT/SIGTERM，Worker 线程仍然阻塞着
    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
}

// ==================== 查询：是否请求了优雅关闭 ====================
// 主循环 while(!isShutdownRequested()) { ... }
bool SignalHandler::isShutdownRequested() {
    // load() = 原子读取，memory_order_acquire 保证之后的所有内存操作不会被重排到这句之前
    return g_shutdown_flag_.load(std::memory_order_acquire);
}

// ==================== 主动请求关闭（代码里想主动优雅退出时调）====================
void SignalHandler::requestShutdown() {
    // 和信号回调做完全一样的事：原子写 true
    g_shutdown_flag_.store(true, std::memory_order_release);
}