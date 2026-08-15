// =========================================
// 8.0 版本：信号处理 + 优雅关闭
// 作用：
//   1. 忽略 SIGPIPE（防止写已断开的 socket 时程序被内核杀掉）
//   2. 捕获 SIGINT(Ctrl+C) / SIGTERM(kill)，设置优雅关闭标志
//   3. 对外提供 isShutdownRequested() 接口，供主循环检测是否要退出
// 设计原则：高内聚，本文件只负责「信号」这一件事
// =========================================
#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>     // sigaction, SIGINT, SIGTERM, SIGPIPE
#include <atomic>       // std::atomic_bool（多线程安全的布尔标志）

// ==================== SignalHandler 类（纯静态，相当于一个命名空间）====================
// 为什么用类包一层？
//   因为 atomic 不能放在全局区随便被别人改，
//   包成类后，外部只能通过 init() / allowSignals() / isShutdownRequested() 接口访问，安全可控
class SignalHandler {
    public:
        // ==================== 初始化信号处理器（必须在 main 开头最早调用！）====================
        // 做三件事：
        //   1. SIG_IGN 忽略 SIGPIPE：防止写已断开的 socket 时进程被杀
        //   2. 注册 SIGINT/SIGTERM 的自定义处理函数：收到信号后把 g_shutdown_flag 置 true
        //   3. 【关键】阻塞 SIGINT/SIGTERM：这样 Worker 线程无法接收这些信号，
        //      确保信号只被主线程接收（在 allowSignals() 中解除阻塞）
        //
        // 调用时机：必须在创建线程（Worker）之前调用，这样 Worker 线程继承的是「已阻塞」的信号掩码
        static void init();

        // ==================== 解除主线程的信号阻塞（必须在进入事件循环前调用！）====================
        // init() 中阻塞了 SIGINT/SIGTERM，Worker 线程继承了阻塞掩码，无法接收这些信号
        // 主线程必须在进入 epoll_wait 事件循环前调用此函数解除阻塞
        // 这样 Ctrl+C / kill 信号就【只会】被主线程接收，主线程的 epoll_wait 会立即被打断
        //
        // 调用时机：在 runServer6_0() 中，进入 while 主循环之前
        static void allowSignals();

        // ==================== 主循环检测：是否请求了优雅关闭 ====================
        // 返回 true 表示收到 SIGINT 或 SIGTERM，该退出了
        static bool isShutdownRequested();

        // ==================== 手动请求关闭（代码里想主动关程序时调）====================
        // 比如 HTTP 管理接口收到 /shutdown 时，可以调用这个函数触发和 Ctrl+C 一样的流程
        static void requestShutdown();

    private:
        // ==================== 信号回调函数（内核收到信号后自动调）====================
        // 参数 signum = 哪个信号触发的（SIGINT=2 / SIGTERM=15）
        // 注意：信号处理函数里只能做「异步信号安全」的操作，
        //       所以我们只写一个 atomic 变量，绝对不调 printf/LOG/malloc 这类非安全函数！
        static void onSignal(int signum);

        // 优雅关闭标志：
        //   atomic = CPU 指令级原子操作，多线程同时读写不会出现读到一半的值
        //   false = 正常运行
        //   true  = 收到退出信号，该走优雅关闭流程了
        static std::atomic<bool> g_shutdown_flag_;
};

#endif  // SIGNAL_HANDLER_H