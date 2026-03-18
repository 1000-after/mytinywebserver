#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>

class Config{
    private:
        static Config* instance;    //单例实例
        std::map<std::string, std::string> config_map;  //配置键值对

        Config();   //私有构造函数
    
    public:
    // 删除拷贝构造和赋值
    Config(const Config&) = delete;
    Config& operator = (const Config&) = delete;

    //获取单例实例
    static Config& getInstance();

    //从文件加载配置
    bool loadFromFile(const std::string& filename);

    //获取配置值
    std::string getString(const std::string&key, const std::string& default_value = "");
    int getInt(const std::string& key, int default_value = 0);
    bool getBool(const std::string& key, bool default_value = false);


    //设置配置值
    void setString(const std::string& key, const std::string& value);

    //保存配置到文件
    bool saveToFile(const std::string& filename);

    //打印所有配置(调试用)
    void print() const;

    //便携访问函数
    int getPort(){return getInt("server.port", 8080);}
    std::string getRootDir() {return getString("server.root_dir", "./www");}
    std::string getLogLevel(){ return getString("log.level", "INFO");}
    int getMaxConnections(){ return getInt("server.max_connections", 1000);}
    int getBacklog(){ return getInt("server.backlog", 128);}
    bool getEnableAccessLog(){ return getBool("security.enable_access_log", true);}
};

#endif // CONFIG_H