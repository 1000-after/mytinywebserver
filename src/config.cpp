// =========================================
// 9.2 配置中心实现
// =========================================
#include "config.h"
#include <fstream>      // std::ifstream 读文件
#include <sstream>      // std::stoi 字符串转 int

// ==================== 单例 ====================
Config& Config::instance() {
    static Config inst;     // C++11 起，局部 static 线程安全初始化
    return inst;
}

// ==================== 辅助函数：去掉字符串首尾空白 ====================
// 例: "  8080  " → "8080"
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if(start == std::string::npos) return "";      // 全是空白
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ==================== 加载配置文件 ====================
bool Config::load(const std::string& path) {
    std::ifstream fin(path);
    if(!fin.is_open()) {
        return false;       // 文件打不开（路径错/权限不够）
    }

    std::string line;
    while(std::getline(fin, line)) {
        line = trim(line);

        // 跳过空行和注释行（# 开头）
        if(line.empty() || line[0] == '#') continue;

        // 按 '=' 分割成 key 和 value
        size_t pos = line.find('=');
        if(pos == std::string::npos) continue;     // 没有 =，格式不对，跳过

        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        if(!key.empty()) {
            items_[key] = val;     // 存进 map
        }
    }
    return true;
}

// ==================== 读字符串 ====================
std::string Config::getString(const std::string& key, const std::string& default_val) const {
    auto it = items_.find(key);
    if(it == items_.end()) return default_val;     // key 不存在，返回默认值
    return it->second;
}

// ==================== 读 int ====================
int Config::getInt(const std::string& key, int default_val) const {
    auto it = items_.find(key);
    if(it == items_.end()) return default_val;
    try {
        return std::stoi(it->second);              // 字符串转 int
    } catch(...) {
        return default_val;                        // 转换失败（如 "abc"），返回默认值
    }
}

// ==================== 读 bool ====================
// 支持: true/false、1/0、yes/no（大小写敏感，配置文件里建议小写）
bool Config::getBool(const std::string& key, bool default_val) const {
    auto it = items_.find(key);
    if(it == items_.end()) return default_val;
    const std::string& v = it->second;
    if(v == "true" || v == "1" || v == "yes")  return true;
    if(v == "false" || v == "0" || v == "no")  return false;
    return default_val;
}