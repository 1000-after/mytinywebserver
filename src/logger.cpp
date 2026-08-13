// =========================================
// 高性能异步日志系统实现
// 采用生产者-消费者模型：业务线程只入队，后台线程负责写盘
// =========================================
#include "logger.h"        // 本项目的日志系统头文件（Logger 类声明、LogConfig、LogEntry、LOG_* 宏）
#include <unistd.h>         // POSIX 标准头文件（提供 time 等系统调用相关声明）
#include <sys/stat.h>       // 文件状态头文件（用到了 mkdir 创建日志目录）
#include <cstdarg>          // C 风格可变参数支持（va_list/va_start/va_end/vsnprintf）

// ==================== 单例实现 ====================
Logger::Logger()                                   // Logger 类的默认构造函数
    : running_(false),                             // 成员初始化：后台线程未启动
      log_file_(nullptr),                          // 成员初始化：文件指针为空
      current_file_size_(0),                       // 成员初始化：当前文件已写字节数清零
      file_seq_(0) {                               // 成员初始化：文件滚动序号清零
}                                                  // 构造函数体为空（全部用初始化列表完成）

Logger::~Logger() {                                // 析构函数
    shutdown();                                    // 调用 shutdown 释放线程和文件资源（RAII 思想）
}

Logger& Logger::instance() {                       // 单例模式获取函数，返回引用
    static Logger instance;                        // C++11 保证局部 static 初始化是线程安全的
    return instance;                               // 返回单例对象的引用
}

// ==================== 初始化 ====================
void Logger::init(const LogConfig& config) {       // 用配置初始化日志系统，必须在 log 前调用
    config_ = config;                              // 保存传入的配置（级别、路径、文件大小上限等）
    running_ = true;                               // 标记后台线程可以运行
    current_file_size_ = 0;                        // 重置当前文件大小计数
    file_seq_ = 0;                                 // 重置文件滚动序号
    
    // 打开日志文件
    if(!config_.file_path.empty()) {               // 只有配置了文件路径才打开
        // 创建日志目录
        std::string dir = config_.file_path.substr(0, config_.file_path.rfind('/'));  // 从路径中截取目录部分
        if(!dir.empty()) {                         // 目录非空才创建
            mkdir(dir.c_str(), 0755);              // 创建目录，权限 755（owner:rwx, group:r-x, other:r-x）
        }
        
        log_file_ = fopen(config_.file_path.c_str(), "a");  // 以追加模式打开文件，不清空原内容
        if(log_file_) {                            // 打开成功时
            // 获取当前文件大小
            fseek(log_file_, 0, SEEK_END);         // 把文件指针定位到末尾
            current_file_size_ = ftell(log_file_);  // ftell 返回当前偏移量，即文件大小
            fseek(log_file_, 0, SEEK_SET);         // 把指针重置回文件开头
        }
    }
    
    // 启动写入线程
    write_thread_ = std::thread(&Logger::writeThread, this);  // 创建后台线程执行 writeThread 成员函数
}

// ==================== 关闭日志系统 ====================
void Logger::shutdown() {
    if(!running_) return;                          // 已经关闭过就直接返回，防止重复关闭
    
    running_ = false;                              // 设置运行标志为 false，告诉后台线程要退出
    cv_.notify_all();  // 通知写入线程退出          // 唤醒所有在 cv_ 上等待的线程（后台写入线程）
    
    if(write_thread_.joinable()) {                 // 如果后台线程还可以 join（未 detach 未 join 过）
        write_thread_.join();                      // 阻塞等待后台线程真正结束
    }
    
    if(log_file_) {                                // 如果文件还开着
        fclose(log_file_);                         // 关闭文件，刷新缓冲区到磁盘
        log_file_ = nullptr;                       // 置空指针，避免悬空指针
    }
}

// ==================== 设置日志级别 ====================
void Logger::setLevel(LogLevel level) {
    config_.level = level;                         // 动态修改日志级别过滤阈值
}

// ==================== 核心：写入日志（立即返回）====================
void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    // 检查日志级别
    if(level < config_.level) return;              // 低于设定级别的日志直接丢弃，零开销过滤
    
    // 构造日志条目
    LogEntry entry;                                // 创建一个日志条目对象
    entry.timestamp = time(nullptr);               // 获取当前 Unix 时间戳（秒级）
    entry.level = level;                           // 保存日志级别
    entry.file = file;                             // 保存源文件名（__FILE__ 宏传入，字符串字面量存静态区）
    entry.line = line;                             // 保存源代码行号（__LINE__ 宏传入）
    
    // 格式化日志消息
    char buf[4096];                                // 栈上缓冲区，避免 malloc，4096 字节足够长
    va_list args;                                  // 声明可变参数列表指针
    va_start(args, fmt);                           // 让 args 指向 fmt 之后的第一个可变参数
    vsnprintf(buf, sizeof(buf), fmt, args);        // 按 fmt 格式化到 buf，最多 4096 字节防溢出
    va_end(args);                                  // 清理 va_list
    entry.message = buf;                           // 把 C 字符串转成 std::string 存入条目
    
    // 特殊处理：FATAL 直接同步写入
    if(level == LOG_FATAL) {                       // 致命错误需要特殊处理
        std::string formatted = formatEntry(entry);  // 立即格式化（不走后台线程）
        if(config_.console_output) {               // 如果允许控制台输出
            fprintf(stderr, "%s", formatted.c_str());  // 写到 stderr（致命错误走标准错误流）
            fflush(stderr);                        // 立即刷新 stderr 缓冲区
        }
        if(log_file_) {                            // 如果文件已打开
            fputs(formatted.c_str(), log_file_);   // 写入文件
            fflush(log_file_);                     // 立即刷新文件缓冲区到磁盘
        }
        return;                                    // 返回（之后宏会调 exit(1) 退出程序）
    }
    
    // 其他级别：异步写入（放入队列）
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁保护队列（RAII，作用域结束自动解锁）
        log_queue_.push(entry);                    // 把日志条目压入队列
    }                                              // 离开作用域，锁自动释放
    cv_.notify_one();                              // 唤醒一个等待的后台写入线程来取日志
}

// ==================== 刷新日志 ====================
void Logger::flush() {
    std::unique_lock<std::mutex> lock(mutex_);     // 加锁（unique_lock 支持 unlock/lock 灵活控制）
    while(!log_queue_.empty()) {                   // 只要队列还有日志就继续处理
        LogEntry entry = log_queue_.front();       // 取出队头日志（拷贝）
        log_queue_.pop();                          // 从队列移除
        lock.unlock();                             // 立即解锁，把 IO 操作放到锁外执行
        
        std::string formatted = formatEntry(entry);  // 格式化日志条目为字符串
        
        if(config_.console_output) {              // 如果允许控制台输出
            // 输出到控制台（带颜色）
            const char* color = "";                // 默认无颜色
            const char* reset = "\033[0m";          // ANSI 重置码，恢复终端默认样式
            switch(entry.level) {                  // 按级别选择颜色
                case LOG_DEBUG: color = "\033[36m"; break;  // 青色
                case LOG_INFO:  color = "\033[32m"; break;  // 绿色
                case LOG_WARN:  color = "\033[33m"; break;  // 黄色
                case LOG_ERROR: color = "\033[31m"; break;  // 红色
                default: break;                   // 其他级别不设颜色
            }
            fprintf(stdout, "%s%s%s", color, formatted.c_str(), reset);  // 输出：颜色+内容+重置
            fflush(stdout);                       // 刷新 stdout 缓冲区，立即显示
        }
        
        if(log_file_) {                            // 如果文件已打开
            fputs(formatted.c_str(), log_file_);   // 写入文件
            fflush(log_file_);                     // 刷新到磁盘（实际内核缓冲）
            current_file_size_ += formatted.size();  // 累加当前文件大小
            
            // 检查是否需要滚动
            if(current_file_size_ >= config_.max_file_size) {  // 文件超过上限，需要滚动
                fclose(log_file_);                 // 关闭当前文件
                file_seq_++;                       // 序号加 1
                char new_path[512];                // 新文件路径缓冲区
                snprintf(new_path, sizeof(new_path), 
                    "%s.%d", config_.file_path.c_str(), file_seq_);  // 拼接新路径，如 app.log.1
                log_file_ = fopen(new_path, "a");  // 以追加模式打开新文件
                current_file_size_ = 0;            // 重置文件大小计数
            }
        }
        
        lock.lock();                               // 重新加锁，准备下一轮取数据
    }                                              // 队列空了退出循环
}

// ==================== 写入线程主函数 ====================
void Logger::writeThread() {
    while(running_) {                              // 主循环：只要 running_ 为 true 就持续运行
        LogEntry entry;                            // 用于保存从队列取出的日志条目
        bool has_data = false;                     // 标记本轮是否取到数据
        
        {
            std::unique_lock<std::mutex> lock(mutex_);  // 加锁访问队列（作用域限制锁范围）
            
            // 等待有日志或系统关闭
            cv_.wait_for(lock, std::chrono::milliseconds(10), [this] {  // 带超时和谓词的等待
                return !log_queue_.empty() || !running_;  // 谓词：队列非空 或 要退出
            });
            
            if(log_queue_.empty()) {               // 被唤醒后队列还是空（超时或虚假唤醒）
                // 没有数据，继续等待
                if(!running_ && log_queue_.empty()) break;  // 要退出且队列空，跳出循环
                continue;                          // 否则继续下一轮等待
            }
            
            // 取一个日志条目
            entry = log_queue_.front();            // 取出队头
            log_queue_.pop();                      // 从队列移除
            has_data = true;                       // 标记取到了数据
        }                                          // 离开作用域，锁释放
        
        if(has_data) {                             // 如果取到了日志
            std::string formatted = formatEntry(entry);  // 格式化日志为字符串
            
            // 输出到控制台
            if(config_.console_output) {           // 如果允许控制台输出
                const char* color = "";            // 默认颜色
                const char* reset = "\033[0m";      // ANSI 重置码
                switch(entry.level) {              // 按级别选颜色
                    case LOG_DEBUG: color = "\033[36m"; break;  // 青色
                    case LOG_INFO:  color = "\033[32m"; break;  // 绿色
                    case LOG_WARN:  color = "\033[33m"; break;  // 黄色
                    case LOG_ERROR: color = "\033[31m"; break;  // 红色
                    default: break;               // 其他不设色
                }
                fprintf(stdout, "%s%s%s", color, formatted.c_str(), reset);  // 输出到终端
                fflush(stdout);                   // 刷新 stdout
            }
            
            // 写入文件
            if(log_file_) {                        // 如果文件已打开
                fputs(formatted.c_str(), log_file_);  // 写入文件
                fflush(log_file_);                 // 刷新文件缓冲
                current_file_size_ += formatted.size();  // 累加文件大小
                
                // 检查是否需要滚动
                if(current_file_size_ >= config_.max_file_size) {  // 超过上限要滚动
                    fclose(log_file_);             // 关闭当前文件
                    file_seq_++;                   // 序号+1
                    char new_path[512];            // 新路径缓冲
                    snprintf(new_path, sizeof(new_path), 
                        "%s.%d", config_.file_path.c_str(), file_seq_);  // 拼接新路径
                    log_file_ = fopen(new_path, "a");  // 打开新文件
                    current_file_size_ = 0;        // 重置大小计数
                }
            }
        }
    }                                              // running_ 变 false 时退出循环
    
    // 退出前处理剩余日志
    flush();                                       // 把队列中残留日志全部写完，保证不丢日志
}

// ==================== 格式化日志条目 ====================
std::string Logger::formatEntry(const LogEntry& entry) {
    char time_str[64];                             // 存放格式化后的时间字符串
    struct tm* tm_info = localtime(&entry.timestamp);  // 把时间戳转成本地时间结构体（带时区）
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);  // 按"年-月-日 时:分:秒"格式化
    
    // 简化文件名（只保留最后部分）
    std::string src_file = entry.file;             // 拷贝出完整路径字符串
    size_t pos = src_file.rfind('/');              // 从右往左查找最后一个 '/'
    if(pos != std::string::npos) {                 // 如果找到了 '/'
        src_file = src_file.substr(pos + 1);       // 截取 '/' 后面的部分，只保留文件名
    }
    
    char buf[4096];                                // 栈上格式化缓冲区
    snprintf(buf, sizeof(buf),                     // 把各字段拼接到 buf
        "[%s] [%s] [%s:%d] %s\n",                  // 格式：[时间] [级别] [文件:行号] 消息\n
        time_str,                                  // 时间字符串
        levelToString(entry.level),                // 级别字符串
        src_file.c_str(),                          // 简化后的文件名
        entry.line,                                // 行号
        entry.message.c_str());                    // 用户消息
    
    return std::string(buf);                       // 转成 std::string 返回
}

// ==================== 日志级别转字符串 ====================
const char* Logger::levelToString(LogLevel level) {
    switch(level) {                                // 根据级别返回对应字符串
        case LOG_DEBUG: return "DEBUG";            // 调试级别
        case LOG_INFO:  return "INFO";             // 信息级别
        case LOG_WARN:  return "WARN";             // 警告级别
        case LOG_ERROR: return "ERROR";            // 错误级别
        case LOG_FATAL: return "FATAL";            // 致命错误级别
        default: return "UNKNOWN";                 // 未知级别
    }
}