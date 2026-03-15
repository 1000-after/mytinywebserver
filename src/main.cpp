#include <iostream>
#include <arpa/inet.h>  //IP地址转换(inet_ntop) + 字节序转换(htons/ntohs)
#include <sys/socket.h> //Linux套接字核心API （socket/bind/listen/accept）
#include <unistd.h> //系统调用(close/read/write)
#include <netinet/in.h>     //网络地址结构体
#include <cstring>  //字符串操作
#include <fstream>  //c++文件输入流(读取本地文件)
#include <sstream>  //字符串流(高i笑傲读取文本文件)
#include <string>   //c++ string类(安全处理字符串)
#include <cerrno>   //系统错误码(errno),获取调用失败原因
#include <cstdio>   //辅助错误打印

//包含日志头文件
#include "simple_logger.h"

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
int main()
{
        logger.info("🚀 启动Web服务器...");
        // ===================== 第一步：创建套接字（类比“装电话机”） =====================
        //cout << "创建电话机" << endl;
        logger.info("创建电话机");
    // socket()：创建TCP套接字
    // 参数1：AF_INET - IPv4协议；参数2：SOCK_STREAM - TCP协议；参数3：0 - 默认协议（TCP）
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){  // 检查套接字创建是否失败
        // strerror(errno)：把系统错误码转成人类可读的字符串（如"Permission denied"）
        //cout << "电话机创建失败: " << strerror(errno) << endl;
        logger.error("电话机创建失败: " + string(strerror(errno)));
        close(server_fd);  // 释放无效资源（即使失败也要尝试关闭）
        return 1;  // 程序异常退出（非0表示出错）
    }

        // ===================== 第二步：设置端口复用（解决“端口被占用”问题） =====================
    // 启用端口复用标记（非0表示启用）
    int opt = 1;
    // setsockopt()：设置套接字选项
    // 参数1：server_fd - 要设置的套接字；参数2：SOL_SOCKET - 操作套接字层；
    // 参数3：SO_REUSEADDR|SO_REUSEPORT - 允许复用地址/端口；
    // 参数4：&opt - 选项值地址；参数5：sizeof(opt) - 选项长度
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt , sizeof(opt)) < 0){
                //cout << "设置套接字选项失败: " << strerror(errno) << endl;
        logger.error("设置套接字选项失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

   // ===================== 第三步：配置服务器地址（类比“给电话机分配号码”） =====================
    // sockaddr_in：IPv4地址结构体，存储服务器IP+端口
    struct sockaddr_in address;
    // memset：清空结构体，避免脏数据（初始化为0）
    memset(&address, 0 , sizeof(address));
    address.sin_family = AF_INET;   // 地址族：IPv4
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡（本地/虚拟机/局域网IP）
// htons()：主机字节序转网络字节序（CPU小端序 → 网络大端序）
    address.sin_port = htons(8080); // 监听8080端口

    //cout << "📡 电话号码设置为8080" << endl;
    logger.info("📡 电话号码设置为8080");

    // ===================== 第四步：绑定套接字（类比“把号码绑定到电话机”） =====================
    // bind()：把地址+端口绑定到套接字
    // 参数1：server_fd - 套接字；参数2：(struct sockaddr*)&address - 通用地址结构体（强制转换）；
    // 参数3：sizeof(address) - 地址结构体长度
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0 ){
        //cout << "❌ 绑定电话号码失败: " << strerror(errno) << endl;
        logger.error("绑定电话号码失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

    // ===================== 第五步：监听端口（类比“开机等来电”） =====================
    // listen()：开始监听端口
    // 参数1：server_fd - 套接字；参数2：3 - 最大等待队列长度（最多3个待处理连接）
    if(listen(server_fd, 3) < 0){
        //cout << "❌ 监听失败: " << strerror(errno) << endl;
        logger.error("监听失败: " + string(strerror(errno)));
        close(server_fd);  // 释放资源
        return 1;  // 异常退出
    }

    // cout << "🔔 电话已开启响铃，等待来电..." << endl;
    // cout << "🌐 请访问: http://localhost:8080/" << endl;
    logger.info("🔔 电话已开启响铃，等待来电...");
    logger.info("🌐 请访问: http://localhost:8080/");

    // ===================== 第六步：循环处理客户端请求（核心循环） =====================
    // while(true)：无限循环，一直处理请求（直到手动终止程序）

    while(true){
        //cout << "\n⏳ 等待客户连接..." << endl;
        logger.info("⏳ 等待客户连接...");

        // 存储客户端的IP+端口信息
        struct sockaddr_in client_addr;
        // 客户端地址结构体长度（必须是变量，accept会修改）
        socklen_t client_len = sizeof(client_addr);

        // accept()：阻塞等待客户端连接
        // 参数1：server_fd - 监听套接字；参数2：&client_addr - 输出客户端地址；
        // 参数3：&client_len - 输入输出地址长度
        // 返回值：client_fd - 客户端连接描述符（每个客户端独立）
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if(client_fd < 0){   // 检查连接是否接受失败
            //cout << "❌ 接电话失败: " << strerror(errno) << endl;
            logger.warning("接电话失败: " + string(strerror(errno)));
            continue;  // 跳过本次循环，继续等待下一个连接
        }

               // ===================== 提取客户端IP和端口（日志打印） =====================
        // 存储客户端IP字符串（INET_ADDRSTRLEN=16，足够存IPv4地址）
        char client_ip[INET_ADDRSTRLEN];
        // inet_ntop()：二进制IP → 字符串IP（如0x7F000001 → "127.0.0.1"）
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        // ntohs()：网络字节序 → 主机字节序（端口转换）
        int client_port = ntohs(client_addr.sin_port);


        // cout << "📞 接到一个电话" << endl;
        // cout << "   📍 客户端IP: " << client_ip << ":" << client_port << endl;
        // cout << "   #️⃣ 客户端编号: " << client_fd << endl;
        logger.info("📞 接到一个电话");
        logger.debug("   📍 客户端IP: " + string(client_ip) + ":" + to_string(client_port));
        logger.debug("   #️⃣ 客户端编号: " + to_string(client_fd));

        // ===================== 读取客户端的HTTP请求 =====================
        // 定义请求缓冲区（4096字节，足够存普通HTTP请求），初始化为0

        char buffer[4096] = {0};
         // read()：读取客户端发送的请求数据
        // 参数1：client_fd - 客户端连接；参数2：buffer - 存储数据；
        // 参数3：sizeof(buffer)-1 - 留1字节存'\0'（避免字符串越界）
        // 返回值：bytes_read - 实际读取的字节数
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);       

        // ===================== 处理读取结果（分支逻辑） =====================
        if(bytes_read > 0){ // 读取到有效数据
            // cout << "📨 客户端请求:" << endl;
            // cout << "--------------------------------" << endl;
            // cout << buffer;  // 打印完整的HTTP请求（如GET /test.html HTTP/1.1）
            // cout << "--------------------------------" << endl;
            logger.debug("📨 客户端请求:");
            logger.debug("--------------------------------");
            // 只打印前200个字符，避免日志太长
            string request_preview(buffer);
            if (request_preview.length() > 200) {
                request_preview = request_preview.substr(0, 200) + "...";
            }
            logger.debug(request_preview);
            logger.debug("--------------------------------");

                    // ===================== 解析HTTP请求路径 =====================
            // 把C风格字符串转成C++ string，方便操作
            string request(buffer);
            string path = "/";  // 默认路径：根路径（访问首页）


            // 提取请求行中的路径（HTTP请求第一行：METHOD PATH VERSION）
            // find(' ')：找第一个空格（如"GET /test.html HTTP/1.1"的第一个空格）
            size_t start = request.find(' ');
            if(start != string::npos){  // 找到第一个空格（string::npos表示没找到）
                // 找第二个空格
                size_t end = request.find(' ', start + 1);
                if(end != string::npos){     // 找到第二个空格
                // substr(起始位置, 长度)：提取路径（如"/test.html?xxx=123"）
                path = request.substr(start + 1, end - start - 1);

                }
            }

            // 核心修复：截断查询参数（去掉?后面的内容，如/test.html?xxx=123 → /test.html）

            size_t query_pos = path.find('?');
            if(query_pos != string::npos){
                path = path.substr(0, query_pos);   //截断路径
                //cout << "🔧 截断查询参数后路径: " << path << endl;
                logger.debug("🔧 截断查询参数后路径: " + path);
            }

            //cout << "📍 请求路径: " << path << endl;
            logger.info("📍 请求路径: " + path);
            // ===================== 拼接文件绝对路径 =====================
            // ！！！替换为你的实际文件根目录绝对路径！！！
            // 示例："/home/你的用户名/test/tinywebserver/www"
            string base_path =  "/home/qianzhu/test/tinywebserver/www";

            // 拼接完整文件路径（如base_path + "/test.html"）
            string filepath = base_path + path;
                 if(path == "/"){  // 如果请求根路径（/）
                filepath = base_path + "/index.html";  // 默认访问首页index.html
            }

            //cout << "📁 尝试打开文件: " << filepath << endl;
            logger.debug("📁 尝试打开文件: " + filepath);
                      // ===================== 读取文件内容 =====================
            // 调用readFile函数，读取文件内容
            string content = readFile(filepath);
            string response; // 存储最终要发送的HTTP响应

            if(!content.empty()){ // 文件读取成功（content非空）
                // 构建200 OK响应
                response = buildResponse(content);
                //cout << "✅ 文件读取成功 (" << content.length() << " 字节)" << endl;
                logger.info("✅ 文件读取成功 (" + to_string(content.length()) + " 字节)");
            }   else{ // 文件读取失败（不存在/权限不足）
                // 构建404错误页面内容
                string error_content = "<h1>404 Not Found</h1><p>请求的文件不存在: " + path + "</p>";
                // 构建404 Not Found响应
                response = buildErrorResponse(404, "Not Found", error_content);
                //cout << "❌ 文件不存在，返回404" << endl;
                logger.warning("❌ 文件不存在，返回404");
            }

                    // ===================== 发送响应给客户端 =====================
            // write()：发送响应数据
            // 参数1：client_fd - 客户端连接；参数2：response.c_str() - C风格字符串；
            // 参数3：response.length() - 要发送的字节数
            // 返回值：send_len - 实际发送的字节数
            ssize_t send_len = write(client_fd, response.c_str(), response.length());
            if(send_len < 0){     // 发送失败
                //cout << "❌ 响应发送失败: " << strerror(errno) << endl;
                logger.error("响应发送失败: " + string(strerror(errno)));
            } else{ // 发送成功
                //cout << "📤 发送响应 (" << send_len << " 字节)" << endl;
                logger.info("📤 发送响应 (" + to_string(send_len) + " 字节)");
                
                // 记录访问日志
                string access_log = "访问日志: " + string(client_ip) + ":" + 
                                   to_string(client_port) + " " + path + " -> " +
                                   (content.empty() ? "404" : "200");
                logger.info(access_log);
            }
        }else if(bytes_read == 0){  // 读取到0字节（客户端主动断开连接）
            //cout << "📭 客户端断开连接（无数据发送）" << endl;
            logger.warning("📭 客户端什么都没说就挂了");
        } else {  // 读取失败（bytes_read < 0，如网络异常）
            //cout << "❌ 数据读取失败: " << strerror(errno) << endl;
            logger.error("数据读取失败: " + string(strerror(errno)));
        }

        // ===================== 关闭客户端连接（释放资源） =====================
        close(client_fd);  // 关闭客户端连接描述符
        //cout << "✅ 电话挂断" << endl;
        logger.info("✅ 电话挂断");

    }

    // 关闭监听套接字（无限循环不会执行到这里，仅作语法补充）
    close(server_fd);
    return 0;  // 程序正常退出
}


// ===================== 辅助函数1：读取文件内容 =====================
string readFile(const string& filepath){
    // ifstream：C++文件输入流，ios::binary - 二进制模式（兼容图片/视频）
    ifstream file(filepath, ios::binary);
    if(!file.is_open()){ // 检查文件是否打开失败
        //cout << "📂 文件打开失败: " << filepath << " 原因: " << strerror(errno) << endl;
        logger.error("📂 文件打开失败: " + filepath + " 原因: " + strerror(errno));
        return "";  // 返回空字符串，表示读取失败
    }

    // stringstream：字符串流，临时存储文件内容（高效）
    stringstream buffer;
    // rdbuf()：获取文件缓冲区，直接读取全部内容（比逐行读快
    buffer << file.rdbuf();
    file.close();  // 显式关闭文件（释放文件句柄）
    // str()：把字符串流转成C++ string返回
    return buffer.str();
}

// ===================== 辅助函数2：构建正常HTTP响应 =====================
string buildResponse(const string& content, const string& contentType){
    // 拼接符合HTTP/1.1协议的响应
    string response = 
    "HTTP/1.1 200 OK\r\n"   // 状态行：HTTP版本+状态码+描述
    "Content-Type: " + contentType + "; charset=utf-8\r\n"  // 内容类型+编码（避免中文乱码）
    "Content-Length: " + to_string(content.length()) + "\r\n"   // 内容长度（浏览器知道读多少字节）
    "Content: close\r\n"    // 短连接（响应后断开）
    "\r\n"  // 空行：分隔响应头和响应体（必须！）
    + content;

    return response;
}

// ===================== 辅助函数3：构建错误HTTP响应 =====================
string buildErrorResponse(int code, const string& message, const string& errorContent){
    // 拼接错误响应（格式和正常响应一致，仅状态行/内容不同）
    string response = 
    "HTTP/1.1 " + to_string(code) +" " + message + "\r\n"   // 状态行（如404 Not Found）
    "Content-Type: text/html; charset=utf-8\r\n"    // 内容类型：HTML
    "Content-Length: " + to_string(errorContent.length()) + "\r\n"  // 错误内容长度
    "Connection: close\r\n"     // 短连接
    "\r\n"      // 空行
    + errorContent;      // 错误响应体

    return response;
}
