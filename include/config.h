// =========================================
// 9.2 配置中心：解析 server.conf，提供类型安全的配置读取
// 设计原则：高内聚，本文件只负责「读配置」这一件事
// 用法：Config::instance().load("config/server.conf");
//       int port = Config::instance().getInt("server.port", 8080);
// =========================================
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>

class Config {
    public:
        // Meyers 单例（和 Logger 一样的风格，线程安全）
        static Config& instance();

        // ==================== 从文件加载配置 ====================
        // path: 配置文件路径（如 "config/server.conf"）
        // 返回: true=加载成功，false=文件打不开
        // 格式: 每行 key = value，# 开头是注释，空行跳过
        bool load(const std::string& path);

        // ==================== 类型安全的 getter ====================
        // key 不存在或转换失败时，返回 default_val（保证永远有值可用）
        std::string getString(const std::string& key, const std::string& default_val) const;
        int         getInt(const std::string& key, int default_val) const;
        bool        getBool(const std::string& key, bool default_val) const;

    private:
        Config() = default;                     // 私有构造，禁外部 new
        ~Config() = default;
        Config(const Config&) = delete;         // 禁拷贝
        Config& operator=(const Config&) = delete;

        // 原始存储：key -> value（都是字符串，getter 时再转类型）
        // key 直接用 "server.port" 这种带点的完整字符串，不做拆分
        std::unordered_map<std::string, std::string> items_;
};

#endif  // CONFIG_H