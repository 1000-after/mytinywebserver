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
#include "signal_handler.h" // 🆕 8.0：信号处理 + 优雅关闭
#include "config.h"         // 🆕 9.2：配置中心

// ==================== 辅助函数：日志级别字符串 → 枚举 ====================
// 配置文件里写的是字符串 "INFO"/"WARN"，代码里用的是枚举 LogLevel，需要转换
static LogLevel parseLogLevel(const std::string& s) {
    if(s == "DEBUG") return LOG_DEBUG;
    if(s == "INFO")  return LOG_INFO;
    if(s == "WARN" || s == "WARNING") return LOG_WARN;
    if(s == "ERROR") return LOG_ERROR;
    if(s == "FATAL") return LOG_FATAL;
    return LOG_INFO;     // 默认 INFO
}

int main() {
    // 🟢 8.0【必须放在最前面！】初始化信号处理器
    //   - 忽略 SIGPIPE（防止写已断开的 socket 时进程被内核杀掉）
    //   - 捕获 SIGINT(Ctrl+C) / SIGTERM(kill 默认发送)，设置优雅关闭标志
    //   必须在创建线程、开监听、初始化日志之前调用！
    //   这样即使初始化过程中用户按 Ctrl+C，信号也能被正确捕获
    SignalHandler::init();

    // 🆕 9.2【必须放在日志初始化之前！】加载配置文件
    //   后续所有模块（日志、线程池、时间轮）都从这里读配置
    //   如果加载失败，会用各 getter 的默认值，程序仍能跑（降级容错）
    if(!Config::instance().load("config/server.conf")) {
        // 配置加载失败不能用 LOG（日志还没初始化），用 printf 兜底
        printf("[警告] 配置文件 config/server.conf 加载失败，将使用默认配置\n");
    }

    // 🆕 初始化日志系统（9.2 改造：从 Config 读，不再硬编码）
    LogConfig log_config;
    log_config.level = parseLogLevel(Config::instance().getString("log.level", "INFO"));
    log_config.console_output = Config::instance().getBool("log.console_output", true);
    log_config.file_path = Config::instance().getString("log.file", "./logs/server.log");
    log_config.max_file_size = Config::instance().getInt("log.max_size", 100 * 1024 * 1024);
    Logger::instance().init(log_config);

    // 🆕 10.0：全局时间轮已改为局部时间轮（每个 Worker 独立实例）
    //   不再需要在此处 init/shutdown，由 Worker::start()/析构自动管理

    LOG_INFO("========================================");
    LOG_INFO("  TinyWebServer 学习项目 - 6.1 HTTP版");
    LOG_INFO("========================================");

    // 9.2 改造：端口从配置文件读（默认 8080）
    int port = Config::instance().getInt("server.port", 8080);

    // 选择服务器版本
    // runServer(8080);      // 5.0 版本：单 Reactor
    runServer6_0(port);     // 6.0 版本：多 Reactor + 线程池（端口从配置读）

    // 关闭日志系统
    Logger::instance().shutdown();
    return 0;
}