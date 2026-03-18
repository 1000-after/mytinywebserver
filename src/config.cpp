#include "config.h"
#include "simple_logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

using namespace std;

//初始化静态成员
Config* Config::instance = nullptr;

//构造函数:设置默认值
Config::Config(){
    //设置默认配置
    config_map = {
        {"server.port", "8080"},
        {"server.root_dir", "./www"},
        {"server.max_connections", "1000"},
        {"server.backlog", "128"},
        {"server.timeout", "30"},

        {"log.level", "INFO"},
        {"log.file", "logs/server.log"},
        {"log.max_size", "10485760"},
        
        {"performance.threads", "4"},
        {"performance.cache_enabled", "true"},
        {"performance.cache_size", "100"},

        {"security.enable_access_log", "true"},
        {"security.allow_directory_listing", "false"}
    };
}

//获取单例实例
Config& Config::getInstance(){
    if(instance == nullptr){
        instance = new Config();
    }
    return *instance;
}

//从文件加载配置
bool Config::loadFromFile(const string& filename){
    ifstream file(filename);
    if(!file.is_open()){
        logger.error("无法打开配置文件: " + filename);
        return false;
    }

    string line;
    int line_num = 0;
    while(getline(file, line)){
        line_num ++;

        //去除前后空格
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t")+1);

        //跳过空行和注释
        if(line.empty() || line[0] == '#'){
            continue;
        }

        //查找等号
        size_t equals_pos = line.find('=');
        if(equals_pos == string::npos){
            logger.warning("配置文件第" + to_string(line_num) + "行格式错误" + line);
            continue;
        }

        //分割键值
        string key = line.substr(0, equals_pos);
        string value = line.substr(equals_pos + 1);

        //去除键值的空格
        key.erase(key.find_last_not_of(" \t") + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        //保存配置
        config_map[key] = value;

        logger.debug("加载配置: " + key + " = " + value);
    }

    file.close();
    logger.info("配置文件加载成功:" + filename);
    return true;
}

//获取字符串配置
string Config::getString(const string& key, const string& default_value){
    auto it = config_map.find(key);
    if(it != config_map.end()){
        return it->second;
    }
    return default_value;
}

//获取整数配置
int Config::getInt(const string& key, int default_value){
    string str_value = getString(key, "");
    if(str_value.empty()){
        return default_value;
    }

    try{
        return stoi(str_value);
    } catch(...){
        logger.error("配置值转换失败: " + key + " = " + str_value);
        return default_value;
    }
}

//获取布尔配置
bool Config::getBool(const string& key, bool default_value){
    string str_value = getString(key, "");
    transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);

    if(str_value == "true" || str_value == "1" || str_value == "yes"){
        return true;
    }else if(str_value == "false" || str_value == "0" || str_value == "no"){
        return false;
    }

    return default_value;
}

//设置配置值
void Config::setString(const string &key, const string& value){
    config_map[key] = value;
}

//保存配置到文件
bool Config::saveToFile(const string& filename){
    ofstream file(filename);
    if(!file.is_open()){
        logger.error("无法创建配置文件: " + filename);
        return false;
    }

    file << "# TinywebServer 配置文件\n";
    file << "#生成事件：" << __DATE__ << __TIME__ << "\n\n";

    for(const auto& [key, value]: config_map){
        file << key << " = " << value << "\n";
    }

    file.close();
    logger.info("配置文件保存成功:" + filename);
    return true;
}

//打印所有配置
void Config::print() const {
    logger.info("=== 当前配置 ===");
    for(const auto& [key, value] : config_map){
        logger.info(key + " = " + value);
    }
    logger.info("================");
}