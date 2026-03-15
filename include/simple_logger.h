#ifndef SIMPLE_LOGGER_H
#define SIMPLE_LOGGER_H

#include <string>

//日志级别
enum LogLevel{
    DEBUG = 0,  //调试信息
    INFO = 1,   //普通信息
    WARNING = 2,    //警告
    ERROR = 3   //错误
};

class SimpleLogger{
    private:
        LogLevel current_level; //当前日志级别
        bool enable_file_log;   //是否启用文件日志

    public:
    //构造函数
    SimpleLogger(LogLevel level = INFO, bool file_log = false);

    //设置日志级别
    void setLevel(LogLevel level);

    //记录日志
    void log(LogLevel level, const std::string& message);

    //便携函数
    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warning(const std::string& msg);
    void error(const std::string& msg);

    //获取当前时间字符串
    static std::string getCurrentTime();

    private:
    //日志级别转字符串
    std::string levelToString(LogLevel level);

    //输出到控制台
    void outputToConsole(const std::string& log_entry);

    //输出到文件
    void outputToFile(const std::string& log_entry);
};
#endif // SIMPLE_LOGGER_H

