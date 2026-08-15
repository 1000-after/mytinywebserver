// #include <iostream>
// #include "server.h"


// int main(){

//     runServer(8080);

//     return 0;
// }

// //6.0
// // =========================================
// // 主程序入口
// // 选择使用哪个版本的服务器
// // =========================================
// #include <iostream>
// #include "server.h" // 服务器声明
// int main() {
//     printf("========================================\n");
//     printf("  TinyWebServer 学习项目\n");
//     printf("========================================\n\n");

//     // 选择服务器版本
//     // runServer(8080);      // 5.0 版本：单 Reactor
//     runServer6_0(8080);     // 6.0 版本：多 Reactor + 线程池

//     return 0;
// }


// =========================================
// 主程序入口
// 选择使用哪个版本的服务器
//7.0添加日志
// =========================================
#include <iostream>
#include "server.h"
#include "logger.h"       // 🆕 引入日志系统
#include "timer_wheel.h"    // 🆕 引入全局时间轮
#include "signal_handler.h" // 🆕 8.0：信号处理 + 优雅关闭

int main() {
    // 🟢 8.0【必须放在最前面！】初始化信号处理器
    //   - 忽略 SIGPIPE（防止写已断开的 socket 时进程被内核杀掉）
    //   - 捕获 SIGINT(Ctrl+C) / SIGTERM(kill 默认发送)，设置优雅关闭标志
    //   必须在创建线程、开监听、初始化日志之前调用！
    //   这样即使初始化过程中用户按 Ctrl+C，信号也能被正确捕获
    SignalHandler::init();

    // 🆕 初始化日志系统
    LogConfig log_config;
    log_config.level = LOG_WARN;       // INFO 及以上级别
    log_config.console_output = true;  // 同时输出到控制台
    log_config.file_path = "./logs/server.log";  // 日志文件路径
    Logger::instance().init(log_config);

    // 🆕 初始化全局时间轮（启动后台滴答线程）
    TimerWheel::instance().init();

    LOG_INFO("========================================");
    LOG_INFO("  TinyWebServer 学习项目 - 6.1 HTTP版");
    LOG_INFO("========================================");

    // 选择服务器版本
    // runServer(8080);      // 5.0 版本：单 Reactor
    runServer6_0(8080);     // 6.0 版本：多 Reactor + 线程池

    // 🆕 关闭全局时间轮（先停滴答线程，再停日志）
    TimerWheel::instance().shutdown();

    // 关闭日志系统
    Logger::instance().shutdown();
    return 0;
}