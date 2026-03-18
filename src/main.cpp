#include <iostream>
#include <arpa/inet.h>  //IP地址转换(inet_ntop) + 字节序转换(htons/ntohs)
#include <sys/socket.h> //Linux套接字核心API （socket/bind/listen/accept）
#include <unistd.h>     //系统调用(close/read/write)
#include <netinet/in.h> //网络地址结构体
#include <cstring>      //字符串操作
#include <fstream>      //c++文件输入流(读取本地文件)
#include <sstream>      //字符串流(高效读取文本文件)
#include <string>       //c++ string类(安全处理字符串)
#include <cerrno>       //系统错误码(errno),获取调用失败原因
#include <cstdio>       //辅助错误打印

//包含日志头文件
#include "simple_logger.h"
//新增不同网页类型文件
#include "mime_types.h"
#include "config.h"

using namespace std;

// 函数声明（先声明后使用，避免编译报错）
// 功能：读取指定路径的文件内容
// 参数：filepath - 文件绝对路径
// 返回：文件内容（空字符串表示读取失败）
string readFile(const string& filepath);

// 功能：构建正常的HTTP响应（200 OK）
// 参数：content - 响应体内容；contentType - 内容类型（默认text/html）
// 返回：完整的HTTP响应字符串
string buildResponse(const string& content, const string& contentType = "text/html");

// 功能：构建错误的HTTP响应（如404/500）
// 参数：code - 状态码；message - 状态描述；errorContent - 错误页面内容
// 返回：完整的HTTP错误响应字符串
string buildErrorResponse(int code, const string& message, const string& errorContent=" ");

//全局日志对象
SimpleLogger logger(INFO, true);    //INFO级别,启用文件日志

// 主函数（程序入口）
int main(int argc, char* argv[])
{

    // 1. 初始化配置
    Config& config = Config::getInstance();
    
    // 2. 解析命令行参数
    string config_file = "config/server.conf";
    if (argc > 1) {
        config_file = argv[1];
        logger.info("使用配置文件: " + config_file);
    }
    
    // 3. 加载配置文件
    if (!config.loadFromFile(config_file)) {
        logger.warning("使用默认配置");
    }
    
    // 4. 打印当前配置
    config.print();
    
    // 5. 从配置获取参数
    int port = config.getPort();
    string root_dir = config.getRootDir();
    int backlog = config.getBacklog();
    bool enable_access_log = config.getEnableAccessLog();
    
        // 尝试打开目录
    FILE* dir = fopen(root_dir.c_str(), "r");
    if (dir == nullptr) {
        logger.error("根目录不存在: " + root_dir);
        return 1;
    } else {
        fclose(dir);
    }
    
    // 7. 记录启动信息
    logger.info("🚀 启动Web服务器...");
    logger.info("监听端口: " + to_string(port));
    logger.info("根目录: " + root_dir);
    logger.info("日志级别: " + config.getLogLevel());
    
    // ===================== 第一步：创建套接字（类比“装电话机”） =====================
    logger.info("创建电话机");
    // socket()：创建TCP套接字
    // 参数1：AF_INET - IPv4协议；参数2：SOCK_STREAM - TCP协议；参数3：0 - 默认协议（TCP）
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){  // 检查套接字创建是否失败
        logger.error("电话机创建失败: " + string(strerror(errno)));
        close(server_fd);  // 释放无效资源（即使失败也要尝试关闭）
        return 1;  // 程序异常退出（非0表示出错）
    }

    // ===================== 第二步：设置端口复用（解决“端口被占用”问题） =====================
    // 启用端口复用标记（非0表示启用）
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt , sizeof(opt)) < 0){
        logger.error("设置套接字选项失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

    // ===================== 第三步：配置服务器地址（类比“给电话机分配号码”） =====================
    // sockaddr_in：IPv4地址结构体，存储服务器IP+端口
    struct sockaddr_in address;
    memset(&address, 0 , sizeof(address));
    address.sin_family = AF_INET;                // 地址族：IPv4
    address.sin_addr.s_addr = INADDR_ANY;        // 监听所有网卡
    address.sin_port = htons(8080);              // 监听8080端口（主机字节序转网络字节序）

    logger.info("📡 电话号码设置为8080");

    // ===================== 第四步：绑定套接字（类比“把号码绑定到电话机”） =====================
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0 ){
        logger.error("绑定电话号码失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

    // ===================== 第五步：监听端口（类比“开机等来电”） =====================
    if(listen(server_fd, 3) < 0){
        logger.error("监听失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

    logger.info("🔔 电话已开启响铃，等待来电...");
    logger.info("🌐 请访问: http://localhost:8080/");

    // ===================== 第六步：循环处理客户端请求（核心循环） =====================
    while(true){
        logger.info("⏳ 等待客户连接...");

        // 存储客户端的IP+端口信息
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // 阻塞等待客户端连接
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if(client_fd < 0){   // 检查连接是否接受失败
            logger.warning("接电话失败: " + string(strerror(errno)));
            continue;  // 跳过本次循环，继续等待下一个连接
        }

        // ===================== 提取客户端IP和端口（日志打印） =====================
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        logger.info("📞 接到一个电话");
        logger.debug("   📍 客户端IP: " + string(client_ip) + ":" + to_string(client_port));
        logger.debug("   #️⃣ 客户端编号: " + to_string(client_fd));

        // ===================== 读取客户端的HTTP请求 =====================
        char buffer[4096] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);       

        // ===================== 处理读取结果（分支逻辑） =====================
        if(bytes_read > 0){ // 读取到有效数据
            // 打印请求预览（避免日志过长）
            logger.debug("📨 客户端请求:");
            logger.debug("--------------------------------");
            string request_preview(buffer);
            if (request_preview.length() > 200) {
                request_preview = request_preview.substr(0, 200) + "...";
            }
            logger.debug(request_preview);
            logger.debug("--------------------------------");

            // ===================== 解析HTTP请求路径 =====================
            string request(buffer);
            string path = "/";  // 默认路径：根路径（访问首页）

            // 提取请求行中的路径（METHOD PATH VERSION）
            size_t start = request.find(' ');
            if(start != string::npos){  
                size_t end = request.find(' ', start + 1);
                if(end != string::npos){     
                    path = request.substr(start + 1, end - start - 1);
                }
            }

            // 截断查询参数（去掉?后面的内容）
            size_t query_pos = path.find('?');
            if(query_pos != string::npos){
                path = path.substr(0, query_pos);
                logger.debug("🔧 截断查询参数后路径: " + path);
            }

            logger.info("📍 请求路径: " + path);

            // ===================== 拼接文件绝对路径 =====================
            // !!! 请确认此路径是你的实际文件目录 !!!
            string base_path =  "/home/qianzhu/test/tinywebserver/www";
            string filepath = base_path + path;
            
            if(path == "/"){  // 如果请求根路径，默认访问index.html
                filepath = base_path + "/index.html";
            }

            logger.debug("📁 尝试打开文件: " + filepath);

            // ===================== 读取文件内容 =====================
            string content = readFile(filepath);
            string response; 
            string content_type = "text/plain"; // 默认类型

            if (!content.empty()) {
                // 提取文件扩展名并获取MIME类型
                size_t dot_pos = filepath.find_last_of('.');
                string ext = "";
                if (dot_pos != string::npos) {
                    ext = filepath.substr(dot_pos + 1); // 提取扩展名
                    content_type = mime_types::get_mime_type(ext); // 获取MIME类型
                }
                
                // 打印MIME类型日志（调试关键）
                logger.info("📄 文件扩展名: " + ext + " → Content-Type: " + content_type);
                
                // 构建响应
                response = buildResponse(content, content_type);
                logger.info("✅ 文件读取成功 (" + to_string(content.length()) + " 字节), Content-Type: " + content_type);
            } else { 

                // 1. 先尝试读取漂亮的404错误页面
                string error_file = base_path   + "/errors/404.html";
                string error_content = readFile(error_file);

                if(error_content.empty()){
                // 文件不存在，构建404响应
                string error_content = 
                    "<!DOCTYPE html>"
                    "<html>"
                    "<head><title>404 Not Found</title></head>"
                    "<body>"
                    "<h1>404 Not Found</h1>"
                    "<p>请求的文件不存在: " + path + "</p>"
                    "<p><a href='/'>返回首页</a></p>"
                    "</body></html>";
                }else{
            // 3. 错误页面文件存在，我们需要动态替换里面的路径信息
        
        // 3.1 查找HTML中显示路径的位置
        // 我们在404.html中有一个 <span id="request-path">这里</span>
        // 要找到这个span，把"这里"替换成实际的路径
        
        // 3.2 在HTML字符串中查找 id="request-path"
        size_t pos = error_content.find("id=\"request-path\"");

        if(pos != string::npos){
            // 3.3 找到了，现在要定位到 > 和 < 之间的文本
            // <span id="request-path">这里</span>
            //                    start↑     end↑
            
            // 找到 > 字符的位置（开始标签的结束）
            size_t start = error_content.find(">", pos + 1);
             // 找到 < 字符的位置（结束标签的开始）
            size_t end = error_content.find("<", start);

             // 3.4 确保找到了正确的边界
            if(start != string::npos && end!= string::npos){
                           // 3.5 替换文本
                // replace(开始位置, 要替换的长度, 新字符串)
                    error_content.replace(start, end-start, path);
            }

        }

                }
                response = buildErrorResponse(404, "Not Found", error_content);
                logger.warning("❌ 文件不存在，返回404: " + filepath);
            }

            // ===================== 发送响应给客户端 =====================
            ssize_t send_len = write(client_fd, response.c_str(), response.length());
            if(send_len < 0){     // 发送失败
                logger.error("响应发送失败: " + string(strerror(errno)));
            } else{ // 发送成功
                logger.info("📤 发送响应 (" + to_string(send_len) + " 字节)");
                
                // 记录访问日志
                string access_log = "访问日志: " + string(client_ip) + ":" + 
                                   to_string(client_port) + " " + path + " -> " +
                                   (content.empty() ? "404" : "200");
                logger.info(access_log);
            }
        } else if(bytes_read == 0){  // 客户端主动断开连接
            logger.warning("📭 客户端什么都没说就挂了");
        } else {  // 读取失败（网络异常等）
            logger.error("数据读取失败: " + string(strerror(errno)));
        }

        // ===================== 关闭客户端连接（释放资源） =====================
        close(client_fd);
        logger.info("✅ 电话挂断");
    }

    // 关闭监听套接字（无限循环不会执行到这里）
    close(server_fd);
    return 0;
}

// ===================== 辅助函数1：读取文件内容 =====================
string readFile(const string& filepath){
    ifstream file(filepath, ios::binary);
    if(!file.is_open()){ 
        logger.error("📂 文件打开失败: " + filepath + " 原因: " + strerror(errno));
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

// ===================== 辅助函数2：构建正常HTTP响应 =====================
string buildResponse(const string& content, const string& contentType){
    string response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + contentType + "; charset=utf-8\r\n"
        "Content-Length: " + to_string(content.length()) + "\r\n"
        "Connection: close\r\n"  // ✅ 修复：正确的短连接字段
        "\r\n"                   // ✅ 必须的空行（分隔响应头和响应体）
        + content;

    return response;
}

// ===================== 辅助函数3：构建错误HTTP响应 =====================
string buildErrorResponse(int code, const string& message, const string& errorContent){
    string response = 
        "HTTP/1.1 " + to_string(code) + " " + message + "\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + to_string(errorContent.length()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + errorContent;

    return response;
}