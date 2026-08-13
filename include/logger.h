// =========================================
// 高性能异步日志系统
// 特点：
// 1. 异步写入：日志写入独立线程，不阻塞业务线程
// 2. 无锁队列：基于环形缓冲区的 lock-free 队列
// 3. 日志分级：DEBUG/INFO/WARN/ERROR/FATAL
// 4. 宏封装：LOG_INFO, LOG_ERROR 等
// =========================================
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>   // 🟢 bug2 修复：std::thread 头文件
#include <string>


// ==================== 日志级别枚举 ====================
enum LogLevel{
    LOG_DEBUG = 0,  // 调试信息
    LOG_INFO = 1,   // 普通信息
    LOG_WARN = 2,   // 警告
    LOG_ERROR = 3,  // 错误
    LOG_FATAL = 4   // 致命错误
};

// ==================== 日志配置 ====================
struct LogConfig{
    LogLevel level; // 日志级别
    bool console_output;    // 是否同时输出到控制台
    std::string file_path;  // 日志文件路径（空字符串则不写文件）
    size_t max_file_size;   // 单个日志文件最大大小（字节）
    int thread_pool_size;   // 日志线程数（默认1）

    LogConfig()
        :level(LOG_INFO),  // 🟢 bug1 修复：Level → level（成员变量名是小写）
        console_output(true),
        file_path("./logs/server.log"),
        max_file_size(100 * 1024 * 1024),
        thread_pool_size(1){

        }
};

// ==================== 日志条目 ====================
struct LogEntry{
    time_t timestamp;   // 时间戳
    LogLevel level; // 日志级别
    std::string file;   // 源文件名
    int line;       // 源文件行号
    std::string message;        // 日志消息

    LogEntry():timestamp(0), level(LOG_INFO), line(0){

    }
};
// ==================== Logger 类（单例）====================
class Logger{
    public:
        //换取单例实例
        static Logger& instance();

        //初始化日志系统
        void init(const LogConfig& config = LogConfig());

        //关闭日志系统
        void shutdown();

        //设置日志级别
        void setLevel(LogLevel level);

        //核心：写入日志(异步，立即返回)
        void log(LogLevel level, const char* file, int line, const char* fmt, ...);

        //刷新日志(阻塞，等待所有日志写入完成)
        void flush();

    private:
        Logger();
        ~Logger();
        Logger(const Logger&) = delete;
        Logger& operator = (const Logger&) = delete;

        //日志写入线程主函数
        void writeThread();

        //格式化日志条目
        std::string formatEntry(const LogEntry& entry);

        //日志界别转字符串
        const char* levelToString(LogLevel level);

        //成员变量
        LogConfig config_;
        std::queue<LogEntry> log_queue_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> running_;
        std::thread write_thread_;
        FILE* log_file_;
        size_t current_file_size_;
        int file_seq_;  // 日志文件序号（滚动用）
};

// ==================== 便捷宏 ====================
// 🔧 修复：不再单独声明 fmt 参数，统一用 ... + __VA_ARGS__
//      这样 LOG_INFO("hello") 展开后不会有多余逗号，纯 c++11 标准也能过
#define LOG_DEBUG(...)  Logger::instance().log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   Logger::instance().log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)   Logger::instance().log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  Logger::instance().log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) \
    do { \
        Logger::instance().log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__); \
        Logger::instance().flush(); \
        exit(1); \
    } while(0)

#endif