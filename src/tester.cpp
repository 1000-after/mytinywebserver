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

// 压测参数
const char* IP = "127.0.0.1";
int PORT = 8080;
int THREAD_COUNT = 100;    // 100 线程 = 100 并发
int REQ_PER_THREAD = 100;  // 每个线程发 100 次

// 全局统计
int success = 0;
int failed = 0;

// 单个线程的压测逻辑
void test_task() {
    char send_buf[] = "hello echo server";
    char recv_buf[1024] = {0};

    for (int i = 0; i < REQ_PER_THREAD; ++i) {
        // 1. 创建 socket
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            failed++;
            continue;
        }

        // 2. 连接服务器
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        inet_pton(AF_INET, IP, &addr.sin_addr);

        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            failed++;
            close(fd);
            continue;
        }

        // 3. 发送数据
        send(fd, send_buf, strlen(send_buf), 0);

        // 4. 接收回显
        recv(fd, recv_buf, sizeof(recv_buf), 0);

        // 5. 完成
        success++;
        close(fd);
    }
}

int main() {
    cout << "=== TCP 压测客户端启动 ===" << endl;
    auto start = chrono::high_resolution_clock::now();

    // 创建多线程并发压测
    vector<thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(test_task);
    }

    // 等待所有线程结束
    for (auto& t : threads) {
        t.join();
    }

    // 统计时间
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> cost = end - start;

    // 输出专业压测结果
    printf("\n===== 压测完成 =====\n");
    printf("总请求数：%d\n", THREAD_COUNT * REQ_PER_THREAD);
    printf("成功：%d\n", success);
    printf("失败：%d\n", failed);
    printf("耗时：%.2f 秒\n", cost.count());
    printf("QPS：%.0f\n", success / cost.count());

    return 0;
}