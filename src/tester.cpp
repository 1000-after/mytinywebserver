//  //1.0/2.0测压
// #include <iostream>
// #include <vector>
// #include <thread>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <chrono>
// #include <string.h>

// using namespace std;

// // 压测参数
// const char* IP = "127.0.0.1";
// int PORT = 8080;
// int THREAD_COUNT = 100;    // 100 线程 = 100 并发
// int REQ_PER_THREAD = 100;  // 每个线程发 100 次

// // 全局统计
// int success = 0;
// int failed = 0;

// // 单个线程的压测逻辑
// void test_task() {
//     char send_buf[] = "hello echo server";
//     char recv_buf[1024] = {0};

//     for (int i = 0; i < REQ_PER_THREAD; ++i) {
//         // 1. 创建 socket
//         int fd = socket(AF_INET, SOCK_STREAM, 0);
//         if (fd < 0) {
//             failed++;
//             continue;
//         }

//         // 2. 连接服务器
//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(PORT);
//         inet_pton(AF_INET, IP, &addr.sin_addr);

//         if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
//             failed++;
//             close(fd);
//             continue;
//         }

//         // 3. 发送数据
//         send(fd, send_buf, strlen(send_buf), 0);

//         // 4. 接收回显
//         recv(fd, recv_buf, sizeof(recv_buf), 0);

//         // 5. 完成
//         success++;
//         close(fd);
//     }
// }

// int main() {
//     cout << "=== TCP 压测客户端启动 ===" << endl;
//     auto start = chrono::high_resolution_clock::now();

//     // 创建多线程并发压测
//     vector<thread> threads;
//     for (int i = 0; i < THREAD_COUNT; ++i) {
//         threads.emplace_back(test_task);
//     }

//     // 等待所有线程结束
//     for (auto& t : threads) {
//         t.join();
//     }

//     // 统计时间
//     auto end = chrono::high_resolution_clock::now();
//     chrono::duration<double> cost = end - start;

//     // 输出专业压测结果
//     printf("\n===== 压测完成 =====\n");
//     printf("总请求数：%d\n", THREAD_COUNT * REQ_PER_THREAD);
//     printf("成功：%d\n", success);
//     printf("失败：%d\n", failed);
//     printf("耗时：%.2f 秒\n", cost.count());
//     printf("QPS：%.0f\n", success / cost.count());

//     return 0;
// }

//3.0测压
// #include <iostream>
// #include <vector>
// #include <thread>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <chrono>
// #include <string.h>

// using namespace std;

// const char* IP = "127.0.0.1";
// int PORT = 8080;
// int THREAD_COUNT = 100;
// int REQ_PER_THREAD = 100;

// int success = 0;
// int failed = 0;

// //核心：拼标准包 4字节长度+真实数据
// void sendStandardPacket(int fd, const char* msg)
// {
//     int dataLen = strlen(msg);
//     //先发4字节int长度
//     send(fd, &dataLen, 4, 0);
//     //再发业务数据
//     send(fd, msg, dataLen, 0);
// }

// void test_task()
// {
//     const char* payload = "hello_official_packet";
//     char recvBuf[2048] = {0};

//     for(int i=0;i<REQ_PER_THREAD;i++)
//     {
//         int fd = socket(AF_INET,SOCK_STREAM,0);
//         if(fd < 0) {failed++;continue;}

//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(PORT);
//         inet_pton(AF_INET,IP,&addr.sin_addr);

//         if(connect(fd,(sockaddr*)&addr,sizeof(addr)) < 0)
//         {
//             failed++;
//             close(fd);
//             continue;
//         }

//         //发合规标准包
//         sendStandardPacket(fd,payload);
//         //收回到显
//         recv(fd,recvBuf,sizeof(recvBuf),0);

//         success++;
//         close(fd);
//     }
// }

// int main()
// {
//     cout << "=== 合规拆包协议-压测客户端启动 ===" << endl;
//     auto start = chrono::high_resolution_clock::now();

//     vector<thread> threads;
//     for(int i=0;i<THREAD_COUNT;i++)
//     {
//         threads.emplace_back(test_task);
//     }

//     for(auto &t : threads) t.join();

//     auto end = chrono::high_resolution_clock::now();
//     chrono::duration<double> cost = end - start;

//     printf("\n===== 压测汇总 =====\n");
//     printf("总请求：%d\n",THREAD_COUNT*REQ_PER_THREAD);
//     printf("成功：%d\n",success);
//     printf("失败：%d\n",failed);
//     printf("耗时：%.2f s\n",cost.count());
//     printf("QPS：%.0f\n",success/cost.count());

//     return 0;
// }
//3.1测压//4.0测压
// #include <iostream>
// #include <vector>
// #include <thread>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <chrono>
// #include <string.h>

// using namespace std;

// const char* IP = "127.0.0.1";
// int PORT = 8080;
// int THREAD_COUNT = 100;
// int REQ_PER_THREAD = 100;

// int success = 0;
// int failed = 0;

// // ==============================
// // ✅ 正确：标准协议发包（匹配服务端）
// // ==============================
// void sendStandardPacket(int fd, const char* msg)
// {
//     // 1. 数据长度（必须用 uint32_t）
//     uint32_t dataLen = strlen(msg);

//     // 2. ✅ 转 网络字节序（必须！必须！必须！）
//     uint32_t netLen = htonl(dataLen);

//     // 3. 先发 4 字节包头
//     send(fd, &netLen, 4, 0);

//     // 4. 再发数据
//     send(fd, msg, dataLen, 0);
// }

// void test_task()
// {
//     const char* payload = "hello_official_packet";
//     char recvBuf[2048] = {0};

//     for(int i=0;i<REQ_PER_THREAD;i++)
//     {
//         int fd = socket(AF_INET,SOCK_STREAM,0);
//         if(fd < 0) {failed++;continue;}

//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(PORT);
//         inet_pton(AF_INET,IP,&addr.sin_addr);

//         if(connect(fd,(sockaddr*)&addr,sizeof(addr)) < 0)
//         {
//             failed++;
//             close(fd);
//             continue;
//         }

//         // ✅ 发正确格式的包
//         sendStandardPacket(fd, payload);
        
//         // 接收回显
//         recv(fd, recvBuf, sizeof(recvBuf), 0);

//         // 计数
//         success++;
//         close(fd);
//     }
// }

// int main()
// {
//     cout << "=== 合规拆包协议-压测客户端启动 ===" << endl;
//     auto start = chrono::high_resolution_clock::now();

//     vector<thread> threads;
//     for(int i=0;i<THREAD_COUNT;i++)
//     {
//         threads.emplace_back(test_task);
//     }

//     for(auto &t : threads) t.join();

//     auto end = chrono::high_resolution_clock::now();
//     chrono::duration<double> cost = end - start;

//     printf("\n===== 压测汇总 =====\n");
//     printf("总请求：%d\n",THREAD_COUNT*REQ_PER_THREAD);
//     printf("成功：%d\n",success);
//     printf("失败：%d\n",failed);
//     printf("耗时：%.2f s\n",cost.count());
//     printf("QPS：%.0f\n",success/cost.count());

//     return 0;
// }

//5.0
// ** 真正的【混合压测】终极版！
// 长连接（心跳） + 短连接（QPS）同时运行！**
#include <iostream>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <string.h>

using namespace std;

const char* IP = "127.0.0.1";
int PORT = 8080;

// ================== 配置 ==================
// 长连接：同时挂多少个客户端（带心跳）
int LONG_CLIENT_COUNT = 50;

// 短连接：压QPS用
int SHORT_THREAD_COUNT = 50;
int SHORT_REQ_PER_THREAD = 200;

// 全局统计
int success = 0;
int failed = 0;

// ==============================
// 发送标准协议包
// ==============================
void sendStandardPacket(int fd, const char* msg)
{
    uint32_t dataLen = strlen(msg);
    uint32_t netLen = htonl(dataLen);
    send(fd, &netLen, 4, 0);
    send(fd, msg, dataLen, 0);
}

// ==============================
// 短连接任务：压 QPS
// ==============================
void short_task()
{
    const char* data = "hello_short";
    char buf[2048] = {0};

    for(int i=0; i<SHORT_REQ_PER_THREAD; i++)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd < 0) { failed++; continue; }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        inet_pton(AF_INET, IP, &addr.sin_addr);

        if(connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            failed++; close(fd); continue;
        }

        sendStandardPacket(fd, data);
        recv(fd, buf, sizeof(buf), 0);

        success++;
        close(fd);
    }
}

// ==============================
// 长连接任务：挂着 + 心跳
// ==============================
void long_task(int id)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { failed++; return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &addr.sin_addr);

    if(connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        failed++; close(fd); return;
    }

    printf("[长连接%d] 已连接\n", id);

    // 死循环：发心跳保活
    while(true)
    {
        sendStandardPacket(fd, "heartbeat");
        sleep(5);
    }

    close(fd);
}

// ==============================
// 主函数：同时启动长连接 + 短连接！
// ==============================
int main()
{
    cout << "===== 混合压测：长连接+心跳 + 短连接压QPS =====\n" << endl;

    // ==========================================
    // 第一步：启动 长连接（后台一直挂着）
    // ==========================================
    vector<thread> long_threads;
    for(int i=0; i<LONG_CLIENT_COUNT; i++)
        long_threads.emplace_back(long_task, i+1);

    // ==========================================
    // 第二步：启动 短连接（压QPS）
    // ==========================================
    auto start = chrono::high_resolution_clock::now();

    vector<thread> short_threads;
    for(int i=0; i<SHORT_THREAD_COUNT; i++)
        short_threads.emplace_back(short_task);

    for(auto& t : short_threads) t.join();

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> cost = end - start;

    // 打印短连接压测结果
    printf("\n===== 短连接压测结果 =====\n");
    printf("总请求：%d\n", SHORT_THREAD_COUNT * SHORT_REQ_PER_THREAD);
    printf("成功：%d\n", success);
    printf("失败：%d\n", failed);
    printf("耗时：%.2f s\n", cost.count());
    printf("QPS：%.0f\n", success / cost.count());

    // 长连接继续挂着，不退出
    for(auto& t : long_threads) t.join();

    return 0;
}