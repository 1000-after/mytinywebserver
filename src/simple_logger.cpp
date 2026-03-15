#include "simple_logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>

using namespace std;

//构造函数
SimpleLogger::SimpleLogger(LogLevel level, bool file_log): current_level(level), enable_file_log(file_log){

}

//设置日志级别
void SimpleLogger::setLevel(LogLevel level){
    current_level = level;
}

//获取当前时间字符串
string SimpleLogger::getCurrentTime(){
    time_t now = time(nullptr);
    tm* local_time = localtime(&now);

    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

    return string(time_str);
}

//日志级别转字符串
string SimpleLogger::levelToString(LogLevel level){
    switch(level){
        case DEBUG: return "[DEBUG]";
        case INFO: return "[INFO]";
        case WARNING: return "[WARN]";
        case ERROR: return "[ERROR]";
        default: return "[UNKNOWN]";
    }
}

//输出到控制台
void SimpleLogger::outputToConsole(const string& log_entry){
    cout << log_entry << endl;
}

//输出到文件
void SimpleLogger::outputToFile(const string& log_entry){
    if(!enable_file_log) return;

    //生成日志文件名(每天一个文件)
    time_t now = time(nullptr);
    tm* local_time = localtime(&now);

    char filename[100];
    strftime(filename, sizeof(filename), "server_%Y%m%d.log", local_time);

    ofstream log_file(filename, ios::app);
    if(log_file.is_open()){
        log_file << log_entry << endl;
        log_file.close();
    }
}

//主日志函数
void SimpleLogger::log(LogLevel level, const string& message){
    // 如果当前日志级别高于要记录的级别，则不记录
    if(level < current_level){
        return;
    }

    //构建日志条目
    string log_entry = getCurrentTime() + " " + levelToString(level) + " " + message;

    //输出
    outputToConsole(log_entry);
    outputToFile(log_entry);
}

//便携函数
void SimpleLogger::debug(const string& message){
    log(DEBUG, message);
}
void SimpleLogger::info(const string& message){
    log(INFO, message);
}
void SimpleLogger::warning(const string& message){
    log(WARNING, message);
}
void SimpleLogger::error(const string& message){
    log(ERROR, message);
}