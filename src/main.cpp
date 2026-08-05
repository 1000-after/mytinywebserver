// #include <iostream>
// #include "server.h"


// int main(){

//     runServer(8080);

//     return 0;
// }

//6.0
// =========================================
// 主程序入口
// 选择使用哪个版本的服务器
// =========================================
#include <iostream>
#include "server.h" // 服务器声明
int main() {
    printf("========================================\n");
    printf("  TinyWebServer 学习项目\n");
    printf("========================================\n\n");

    // 选择服务器版本
    // runServer(8080);      // 5.0 版本：单 Reactor
    runServer6_0(8080);     // 6.0 版本：多 Reactor + 线程池

    return 0;
}