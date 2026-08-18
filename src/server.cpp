//1.0
// #include <stdio.h>  // 标准输入输出：printf, perror（打印错误信息）
// #include <stdlib.h> // 标准库：malloc, free, exit（这里用来处理异常退出）
// #include <string.h> // 字符串操作：memset, strlen（这里用来清空缓冲区）
// #include <unistd.h> // Unix 标准函数：close, read, write（关闭文件描述符、读写数据）

// // 套接字核心：socket, bind, listen, accept, send, recv
// // 作用：创建 TCP 连接、监听、接受客户端、收发数据
// #include <sys/socket.h> 

// // IP地址结构体：sockaddr_in（存放IP、端口、协议族）
// // 作用：给服务器绑定地址端口用
// #include <netinet/in.h> 

// // 文件控制：fcntl
// // 作用：把 socket 设置为【非阻塞模式】，这是 epoll 必须的
// #include <fcntl.h>      

// // 错误号：errno
// // 作用：系统调用失败时，告诉你为什么失败（EAGAIN 等）
// #include <errno.h>  

// // epoll 核心头文件
// // 作用：提供 epoll_create1, epoll_ctl, epoll_wait 三个高并发神器
// #include <sys/epoll.h>  

// #include "server.h"

// // ===================== 宏定义 =====================
// // 最大同时监听的事件数量（一次 epoll_wait 最多返回多少个活跃连接）
// #define MAX_EVENTS 1024

// //读取客户端数据的缓冲区大小
// #define BUF_SIZE 1024

// // ===================== 函数 =====================

// /**
//  * @brief 设置文件描述符为【非阻塞模式】（游双书标准写法）
//  * @param fd 要设置的 socket 文件描述符
//  * @return 原来的状态标志
//  * 作用：让 accept / read / write 不会卡住程序
//  */

// int setnonblocking(int fd)
// {
//     //第一步:获取fd原来的状态
//     int old_flag = fcntl(fd, F_GETFL);

//     //第二步:在原来的状态上 加上 非阻塞标志O_NONBLOCK
//     int new_flag = old_flag | O_NONBLOCK;

//     //第三步:把新状态设置回去
//     fcntl(fd, F_SETFL, new_flag);

//     return new_flag;
// }

// /**
//  * @brief 将一个 fd 添加到 epoll 监听红黑树中，监听【读事件】
//  * @param epoll_fd epoll 实例的文件描述符
//  * @param fd 要监听的 socket（监听fd 或 客户端fd）
//  * 作用：告诉内核：我要监听这个 fd，有数据时通知我
//  */

// static void epollAddFd(int epoll_fd, int fd){
//     //定义epoll事件结构体
//     epoll_event ev;

//     //EPOLLIN = 监听【可读事件】（客户端发来数据）
//     ev.events = EPOLLIN ;

//     //把要监听的fd存进去，内核触发事件时会返回给我们
//     ev.data.fd = fd;

//     //EPOLL_CTL_ADD = 添加fd到epoll树上
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

//     //✅ 关键：所有交给 epoll 管理的 fd 必须是非阻塞
//     setnonblocking(fd);

// }

// /**
//  * @brief epoll 高并发服务器主函数（Reactor 模型）
//  * 流程：创建socket -> 绑定 -> 监听 -> epoll创建 -> 事件循环
//  */

// void runServer(uint16_t ports){
//     // ============= 1. 创建 TCP 监听 socket =============
//     // AF_INET = IPv4
//     // SOCK_STREAM = TCP 协议
//     int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("socket");
//         return;
//     }

//     // 设置端口复用：服务器重启后可以立刻绑定端口，不会提示“地址已使用”
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     // ============= 2. 绑定 IP + 端口 =============
//     sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
//         perror("bind");
//         close(sockfd);
//         return ;
//     }

//     // ============= 3. 开始监听 =============
//     // 5 = 全连接队列长度（最多等待处理的连接数）
//     if(listen(sockfd, 100) < 0){
//         perror("listen");
//         close(sockfd);
//         return;
//     }
    

//     // ============= 4. 创建 epoll 实例 =============
//     // epoll_create1(0) → 创建一个 epoll 句柄（内核会创建一张红黑树）
//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("epoll_create");
//         close(sockfd);
//         return;
//     }

//     // ============= 5. 把【监听fd】加入 epoll =============
//     epollAddFd(epoll_fd, sockfd);

//     // 用来接收 epoll_wait 返回的活跃事件
//     epoll_event events[MAX_EVENTS];

//     printf("Epoll高并发服务启动：8080端口\n");

//     // ============= 6. Reactor 主事件循环（核心！）=============
//     while(true){
//         /**
//          * @brief 阻塞等待事件（没有事件就休眠，不占CPU）
//          * @param -1 表示无限等待
//          * @return nready 活跃的文件描述符数量
//          */
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;


//         // ============= 7. 遍历所有活跃事件 =============
//         for(int i = 0; i < nready; i++){
//             // 拿到触发事件的文件描述符
//             int fd = events[i].data.fd;
//             // ======================
//             // 事件类型1：监听fd有新客户端连接
//             // ======================
//             if(fd == sockfd){
//                 sockaddr_in cli_addr;
//                 socklen_t cli_len = sizeof(cli_addr);

//                 //接收新连接
//                 int conn_fd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);
//                 if(conn_fd < 0) continue;


//                 //把新客户端fd加入epoll监听
//                 epollAddFd(epoll_fd, conn_fd);

//             }

//             // ======================
//             // 事件类型2：客户端发来数据了（可读事件）
//             // ======================
//             else{
//                 char buf[BUF_SIZE] = {0};

//                 // 非阻塞读取客户端数据
//                 ssize_t n = read(fd, buf, BUF_SIZE -1);

//                 // 客户端关闭连接 或 发生错误
//                 if(n <= 0){
//                     //从epoll树上删除
//                     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
//                     //关闭文件描述符(避免fd泄露)
//                     close(fd);
//                     continue;
//                 }

//                  // ======================
//                 // 业务逻辑：回显（收到什么发回去什么）
//                 // ======================
//                 write(fd, buf, n);

//                  // ======================
//                 // 加这 2 行！压测立刻满血！
//                 //短连接测压
//                 // ======================
                
//                 epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
//                 close(fd);
//             }
//         }
//     }
//     // 关闭资源（理论上服务器不会跑到这里）
//      close(epoll_fd);
//     close(sockfd);
//     return;
// }

//2.0
// //2.0
// #include <stdio.h> // 标准输入输出：printf, perror（打印错误信息）
// #include <stdlib.h> // 标准库：malloc, free, exit（这里用来处理异常退出）
// #include <string.h> // 字符串操作：memset, strlen（这里用来清空缓冲区）
// #include <unistd.h> // Unix 标准函数：close, read, write（关闭文件描述符、读写数据）

// // 套接字核心：socket, bind, listen, accept, send, recv
// // 作用：创建 TCP 连接、监听、接受客户端、收发数据
// #include <sys/socket.h>

// // IP地址结构体：sockaddr_in（存放IP、端口、协议族）
// // 作用：给服务器绑定地址端口用
// #include <netinet/in.h>

// // 文件控制：fcntl
// // 作用：把 socket 设置为【非阻塞模式】，这是 epoll 必须的
// #include <fcntl.h>


// // 错误号：errno
// // 作用：系统调用失败时，告诉你为什么失败（EAGAIN 等）
// #include <errno.h>

// // epoll 核心头文件
// // 作用：提供 epoll_create1, epoll_ctl, epoll_wait 三个高并发神器
// #include <sys/epoll.h>

// #include <unordered_map> //c++哈希表(fd找连接)

// #include "server.h"

// #define MAX_EVENTS 1024
// #define BUF_SIZE 1024

// //全局哈希表：保存所有客户端连接(小白最容易理解)
// static std::unordered_map<int, Connection> g_connections;

// //设置非阻塞
// int setnonblocking(int fd){
//     int old_flag = fcntl(fd, F_GETFL);
//     int new_flag = old_flag | O_NONBLOCK;
//     fcntl(fd, F_SETFL, new_flag);
//     return old_flag;
// }

// //添加到epoll（ET模式）
// void epollAddFd(int epoll_fd, int fd){
//     struct epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;
//     ev.data.fd = fd;
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
//     setnonblocking(fd);
// }

// //ET循环读数据->存入缓冲区(解决粘包核心)
// static bool readToBuffer(int fd)
// {
//     // auto = C++11自动推导类型
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()){
//         return false;
//     }
    
//     char tmp[BUF_SIZE];
//     while(1)
//     {
//         ssize_t n = read(fd, tmp, BUF_SIZE);

//         if(n > 0){
//             //把数据放进连接的缓冲区(不直接处理，解决粘包)
//             it->second.read_buf.insert(
//                     it->second.read_buf.end(),
//                     tmp,
//                     tmp + n
//             );
//         } else if(n== 0){
//             return false;   //客户端断开
//         }   else{
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 break;  //数据读完了
//             }
//             return false;
//         }
//     }
//     return true;
// }

// //关闭并清理连接
// static void closeConnection(int epoll_fd, int fd){
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);    
//     close(fd);
//     g_connections.erase(fd);
// }

// //主服务器逻辑
// void runServer(uint16_t ports){
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("socket");
//         return ;
//     }

//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     struct sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr) ) < 0){
//         perror("bind");
//         close(sockfd);
//         return;
//     }


//     if(listen(sockfd, 100) < 0){
//         perror("listen");
//         close(sockfd);
//         return;
//     }

//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("epoll_fd");
//         close(sockfd);
//         return;
//     }

//     epollAddFd(epoll_fd, sockfd);

//     struct epoll_event events[MAX_EVENTS];
//     printf("ET 服务器启动：端口 %d\n", ports);

//     while(1){
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;

//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;

//             //新连接
//             if(fd == sockfd){
//                 //ET必须循环accept
//                 while (1){
//                     struct sockaddr_in cli_addr;
//                     socklen_t cli_len = sizeof(cli_addr);
//                     int conn_fd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);

//                     if(conn_fd < 0){
//                         if(errno == EAGAIN) break;
//                         continue;
//                     }

//                     //新建连接，存入哈希表
//                     Connection conn;
//                     conn.fd = conn_fd;
//                     g_connections[conn_fd] = conn;

//                     epollAddFd(epoll_fd, conn_fd);

//                 }
                
//             }

//             //客户端消息
//             else{
//                 bool ok = readToBuffer(fd);
//                 if(!ok){
//                     closeConnection(epoll_fd, fd);
//                     continue;
//                 }

//                 //打印缓冲区长度(只看效果，不处理业务)
//                 auto& conn = g_connections[fd];
//                 printf("客户端 %d 缓存数据：%zu 字节\n", fd, conn.read_buf.size());
//                 // 回显（压测必须）
//                 write(fd, conn.read_buf.data(), conn.read_buf.size());

//                 // 短连接用完关闭（符合压测方式）
//                 closeConnection(epoll_fd, fd);
//             }
//         }
//     }

//     close(epoll_fd);
//     close(sockfd);
// }

//2.0
// #include <stdio.h> // 标准输入输出：printf, perror（打印错误信息）
// #include <stdlib.h> // 标准库：malloc, free, exit（这里用来处理异常退出）
// #include <string.h> // 字符串操作：memset, strlen（这里用来清空缓冲区）
// #include <unistd.h> // Unix 标准函数：close, read, write（关闭文件描述符、读写数据）


// // 套接字核心：socket, bind, listen, accept, send, recv
// // 作用：创建 TCP 连接、监听、接受客户端、收发数据
// #include <sys/socket.h>

// // IP地址结构体：sockaddr_in（存放IP、端口、协议族）
// // 作用：给服务器绑定地址端口用
// #include <netinet/in.h>


// // 文件控制：fcntl
// // 作用：把 socket 设置为【非阻塞模式】，这是 epoll 必须的
// #include <fcntl.h>

// // 错误号：errno
// // 作用：系统调用失败时，告诉你为什么失败（EAGAIN 等）
// #include <errno.h>


// // epoll 核心头文件
// // 作用：提供 epoll_create1, epoll_ctl, epoll_wait 三个高并发神器
// #include <sys/epoll.h>

// #include <unordered_map>

// #include "server.h"

// #define MAX_EVENTS 1024
// #define BUF_SIZE 1024
// //全局哈希表:保存所有客户端连接(小白最容易理解)
// static std::unordered_map<int, Connection> g_connections;

// //设置非阻塞
// int setnonblocking(int fd){
//     int old_flag = fcntl(fd, F_GETFL);
//     int new_flag = old_flag | O_NONBLOCK;
//     fcntl(fd, F_SETFL, new_flag);
//     return new_flag;
// }

// //添加到epoll（ET模式）
// void epollAddFd(int epoll_fd, int fd){
//     struct epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;
//     ev.data.fd = fd;
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
//     setnonblocking(fd);
// }

// //ET循环读数据->存入缓冲区（解决粘包核心问题）
// static bool readToBuffer(int fd)
// {
//     //auto = C++11自动推导类型
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()){
//         return false;
//     }

//     char tmp[BUF_SIZE];
//     while(1){
//         ssize_t n = read(fd, tmp, BUF_SIZE);

//         if(n > 0){
//             //把数据放进连接的缓冲区(不直接处理，解决粘包)
//             it->second.read_buf.insert(
//                 it->second.read_buf.end(),
//                 tmp,
//                 tmp + n
//             );
//         } else if( n == 0){
//             return false;   //客户端断开
//         } else {
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 break;  //数据读完了
//             }
//             return false;
//         }
//     }
//     return true;
// }

// //关闭并清理连接
// static void closeConnection(int epoll_fd, int fd){
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);    //C++11 空指针
//     close(fd);
//     g_connections.erase(fd);
// }

// //会先操作
// static void writeback(int epoll_fd, int fd, Connection conn){
//     write(fd, conn.read_buf.data(), conn.read_buf.size());
//     closeConnection(epoll_fd, fd);
// }

// //主服务器逻辑
// void runServer(uint16_t ports){
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("sockfd");
//         return;
//     }

//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     struct sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
//         perror("bind");
//         close(sockfd);
//         return ;
//     }

//     if(listen(sockfd, 100) < 0){
//         perror("listen");
//         close(sockfd);
//         return;
//     }

//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("epoll_fd");
//         close(sockfd);
//         return;
//     } 

//     epollAddFd(epoll_fd, sockfd);

//     struct epoll_event events[MAX_EVENTS];
//     printf("ET 服务器启动：端口 %d\n", ports);

//     while(1){
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;

//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;

//             //新连接
//             if(fd == sockfd){
//                 //ET必须循环 accept
//                 while(1){
//                     struct sockaddr_in cli_addr;
//                     socklen_t cli_len = sizeof(cli_addr);

//                     int conn_fd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);

//                     if(conn_fd < 0){
//                         if(errno == EAGAIN) break;
//                         continue;
//                     }

//                     //新建连接,存入哈希表中
//                     Connection conn;
//                     conn.fd = conn_fd;
//                     g_connections[conn_fd] = conn;

//                     epollAddFd(epoll_fd, conn_fd);

//                 }
//             }//客户端消息
//             else{
//                 bool ok = readToBuffer(fd);
//                 if(!ok){
//                     closeConnection(epoll_fd, fd);
//                     continue;
//                 }

//                 //打印缓冲区长度(只看效果，不处理业务)
//                 auto& conn = g_connections[fd];
//                 printf("客户端 %d 缓存数据：%zu 字节\n", fd, conn.read_buf.size());
//                 writeback(epoll_fd, fd, g_connections[fd]);
//             }
//         }
//     }
//     close(epoll_fd);
//     close(sockfd);
//     return;
// }

// //3.0添加拆包代码
// #include <stdio.h>  // 标准输入输出：printf, perror（打印错误信息）
// #include <stdlib.h> // 标准库：malloc, free, exit（这里用来处理异常退出）
// #include <string.h> // 字符串操作：memset, strlen（这里用来清空缓冲区）
// #include <unistd.h> // Unix 标准函数：close, read, write（关闭文件描述符、读写数据）

// // 套接字核心：socket, bind, listen, accept, send, recv
// // 作用：创建 TCP 连接、监听、接受客户端、收发数据
// #include <sys/socket.h>


// // IP地址结构体：sockaddr_in（存放IP、端口、协议族）
// // 作用：给服务器绑定地址端口用
// #include <netinet/in.h>


// // 文件控制：fcntl
// // 作用：把 socket 设置为【非阻塞模式】，这是 epoll 必须的
// #include <fcntl.h>


// // 错误号：errno
// // 作用：系统调用失败时，告诉你为什么失败（EAGAIN 等）
// #include <errno.h>


// // epoll 核心头文件
// // 作用：提供 epoll_create1, epoll_ctl, epoll_wait 三个高并发神器
// #include <sys/epoll.h>

// #include <unordered_map>

// #include "server.h"

// #define MAX_EVENTS 1024
// #define BUF_SIZE 1024

// //全局哈希表:保存所有客户会断连接
// static std::unordered_map<int,Connection> g_connections;

// //设置非阻塞
// int setnonblocking(int fd){
//     int old_flag = fcntl(fd, F_GETFL);
//     int new_flag = old_flag | O_NONBLOCK;
//     fcntl(fd, F_SETFL, new_flag);
//     return old_flag;
// }

// //添加到epoll中(ET模式)
// void epollAddFd(int epoll_fd, int fd){
//     struct epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;
//     ev.data.fd = fd;
//     epoll_ctl(epoll_fd,EPOLL_CTL_ADD, fd, &ev);

//     //顺带设置非阻塞
//     setnonblocking(fd);
// }

// // ==============================================
// // 【底层拆包：包头】
// // 只定义“数据长度”，和业务无关！
// // ==============================================
// struct PacketHeader{
//     int data_len;   //数据长度(4字节)
// };

// // ==============================================
// // 【核心：纯底层拆包函数】
// // 功能：从缓冲区取出一个完整包
// // 无业务！无逻辑！只切数据包！
// // ==============================================

// static bool parseOnePacket(Connection& conn){
//     //1.缓冲区连包头都不够->半包，直接返回
//     if(conn.read_buf.size() < sizeof(PacketHeader)){
//         return false;
//     }

//     //2.取出包头，拿到数据长度
//     // 2. 关键：获取包头
//     // data()返回char*，强制转为PacketHeader*
//     // 这样可以直接读取包头字段
//     PacketHeader* header = (PacketHeader*)conn.read_buf.data();
//     // 3. 计算总包长
//     int total_packet_len = sizeof(PacketHeader) + header->data_len;

//     //3.缓冲区不够完整包->半包
//     if(conn.read_buf.size() < total_packet_len){
//         return false;   // 包体不完整
//     }

//     // ==================================================
//     // ✅ 拆包成功！在这里回显给客户端（压测专用）
//     // ==================================================
//     // 5. 获取数据部分指针
//     // 跳过包头大小，指向数据开始
//     char* data_ptr = conn.read_buf.data() + sizeof(PacketHeader);
//     // 6. 回显数据
//     write(conn.fd, data_ptr, header->data_len); // 👈 回显代码

//     printf("[回显成功] 客户端=%d, 数据长度=%d\n",
//            conn.fd, header->data_len);

//     // 7. 删除已处理的包
//     //4.把意见处理完的包,从缓冲区删除(剩下的数据留着下次用)
//     conn.read_buf.erase(
//         conn.read_buf.begin(),
//         conn.read_buf.begin() + total_packet_len
//     );

//     return true;
// }

// //ET循环读数据->存入缓冲区
// static bool readToBuffer(int fd){
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()){
//         return false;
//     }

//     char tmp[BUF_SIZE];
//     while(1){
//         ssize_t n = read(fd, tmp, BUF_SIZE);

//         if(n > 0){
//             it -> second.read_buf.insert(
//                 it->second.read_buf.end(),
//                 tmp,
//                 tmp + n
//             );
            
//         }else if(n == 0){
//             return false;
//         }else{
//             if(errno == EAGAIN || errno == EWOULDBLOCK)
//             {
//                 break;
//             }
//             return false;
//         }
//     }

//     // ==============================================
//     // 【关键】读完数据 → 开始拆包（循环拆完所有完整包）
//     // ==============================================
//     while(parseOnePacket(it->second));
//     return true;
// }

// //关闭并清理连接
// static void closeConnection(int epoll_fd, int fd){
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
//     close(fd);
//     g_connections.erase(fd);
// }


// //主服务器
// void runServer(uint16_t ports){
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("socket");
//         return;
//     }
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     struct sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);


//     //绑定地址
//     if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
//     {
//         perror("bind");
//         close(sockfd);
//         return;
//     }

//     //监听
//     if(listen(sockfd, 100) < 0){
//         perror("listen");
//         close(sockfd);
//         return;
//     }

//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("epoll_fd");
//         close(sockfd);
//         return;
//     }

//     epollAddFd(epoll_fd, sockfd);

//     //创建存储地址
//     struct epoll_event events[MAX_EVENTS];
//     printf("ET 服务器(带拆包)启动：端口 %d\n", ports);


//     while(1){
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;

//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;


//             //新连接
//             if(fd == sockfd){
//                 //ET模式一次性读全连接
//                 while(1){
//                     struct sockaddr_in cli_addr;
//                     socklen_t  cli_len = sizeof(cli_addr);
//                     int conn_fd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);

//                     if(conn_fd < 0){
//                         if(errno == EAGAIN) break;
//                         continue;
//                     }
//                     Connection conn;
//                     conn.fd = conn_fd;
//                     g_connections[conn_fd] = conn;

//                     epollAddFd(epoll_fd, conn_fd);

//                 }

//             }
//              //客户端消息
//                 else {

//                     bool ok = readToBuffer(fd);
//                     if(!ok){
//                         closeConnection(epoll_fd, fd);
//                         continue;
//                     }
//                 }

//         }


        
//     }

//     close(epoll_fd);
//     close(sockfd);
// }


// ==============================================
// 3.1 工业级完整版
// 功能：epoll ET模式 + 非阻塞socket + 读缓冲区 + 标准拆包协议
// 修复：字节序、内存对齐、非法包检查、协议规范
// ==============================================
// #include <stdio.h> // 输入输出：printf打印日志、perror打印系统错误
// #include <stdlib.h> // 通用工具：内存分配、退出
// #include <string.h> // 内存操作：memset、strlen
// #include <unistd.h> // Unix系统调用：close、read、write

// // Linux网络编程核心头文件
// #include <sys/socket.h> // socket、bind、listen、accept、send、recv
// #include <netinet/in.h> // sockaddr_in结构体、htons/ntohl字节序转换
// #include <fcntl.h> // fcntl：设置非阻塞模式
// #include <errno.h> // 错误码：EAGAIN、EWOULDBLOCK等
// #include <sys/epoll.h> // epoll_create1、epoll_ctl、epoll_wait

// // C++容器
// #include <unordered_map> // 哈希表：快速查找客户端连接

// // 自定义头文件（Connection结构体定义）
// #include <server.h>

// // ==============================================
// // 宏定义（全局配置）
// // ==============================================
// #define MAX_EVENTS 1024 // epoll_wait一次最多返回多少个事件
// #define BUF_SIZE 1024 // 每次从socket读取的临时缓冲区大小
// #define MAX_PACKET_SIZE 65536 // 最大允许的数据包大小（64KB），防恶意攻击

// // ==============================================
// // 全局变量
// // ==============================================
// // 静态全局哈希表：存储所有客户端连接
// // key：文件描述符fd
// // value：Connection结构体（包含fd、读缓冲区）
// static std::unordered_map<int,Connection> g_connections;


// // ==============================================
// // 函数：将fd设置为非阻塞模式
// // 作用：ET模式必须搭配非阻塞socket，否则会卡死
// // ==============================================
// int setnonblocking(int fd){
//      // 第一步：获取fd原来的flag（状态标记）
//      int old_flag = fcntl(fd, F_GETFL);
//      int new_flag = old_flag | O_NONBLOCK;
//      fcntl(fd, F_SETFL, new_flag);
//      return old_flag;
// }

// // ==============================================
// // 函数：将fd添加到epoll监听
// // 模式：ET边缘触发 + 只监听读事件EPOLLIN
// // ==============================================
// void epollAddFd(int epoll_fd, int fd){
//     struct epoll_event ev;
    
//     // ET模式 + 监听可读事件
//     ev.events = EPOLLIN | EPOLLET;

//     // 事件关联的数据：只需要存fd
//     ev.data.fd = fd;

//     // 添加到epoll监听列表
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    

//     // 顺手设置非阻塞（ET必须非阻塞）
//     setnonblocking(fd);
// }


// // ==============================================
// // 【工业级协议头】
// // 1. #pragma pack(1) 强制1字节对齐 → 不填充空白字节
// // 2. uint32_t 固定4字节 → 跨平台不变
// // 3. 网络传输必须用：包头(4字节) + 数据体
// // ==============================================
// #pragma pack(push,1)
// struct PacketHeader{
//     uint32_t data_len;  // 数据体长度（必须是4字节无符号整数）
// };
// #pragma pack(pop)
// // ==============================================

// // ==============================================
// // 【核心拆包函数】
// // 功能：从缓冲区里拆出一个完整的包
// // 返回值：true=拆到一个完整包 false=数据不够/非法包
// // ==============================================
// static bool parseOnePacket(Connection& conn){
//        // ----------------------
//     // 步骤1：判断是否够读包头（4字节）
//     // ----------------------
//     if(conn.read_buf.size() < sizeof(PacketHeader)){
//         return false;   //包头都不够->半包，等下次
//     }

//     // ----------------------
//     // 步骤2：从缓冲区头部强转成包头指针
//     // ----------------------
//     PacketHeader* header = (PacketHeader*)conn.read_buf.data();

//     // ----------------------
//     // 步骤3：网络字节序 → 转主机字节序（必须转！否则数字是乱的）
//     // ----------------------
//     uint32_t data_len = ntohl(header->data_len);

//      // ----------------------
//     // 步骤4：安全检查：防止恶意超长包/空包
//     // ----------------------
//     if(data_len == 0 || data_len > MAX_PACKET_SIZE){
//         printf("[错误] 非法包长度：%u\n", data_len);
//         return false;
//     }

//     // ----------------------
//     // 步骤5：计算一个完整包的总长度
//     // 总长度 = 包头4字节 + 数据长度
//     // ----------------------
//     uint32_t total_len = sizeof(PacketHeader) + data_len;

//     // ----------------------
//     // 步骤6：判断缓冲区是否够一个完整包
//     // ----------------------
//     if(conn.read_buf.size() < total_len){
//         return false;   //不够->半包
//     }

//     // ----------------------
//     // ✅ 到这里：已经确定收到一个完整合法包
//     // ----------------------

//     // 数据指针 = 缓冲区起始 + 跳过包头4字节
//     char* data_ptr = conn.read_buf.data() + sizeof(PacketHeader);

//     // 回显给客户端：只发送真实数据部分
//     write(conn.fd, data_ptr, data_len);

//     //打印日志
//     printf("[合法拆包] fd=%d 数据长度=%u\n", conn.fd, data_len);

//     // ----------------------
//     // 步骤7：把已经处理完的包从缓冲区删掉
//     // ----------------------
//     conn.read_buf.erase(
//         conn.read_buf.begin(),  //从开头删
//         conn.read_buf.begin() + total_len // 删到整个包结束
//     );
//     return true;    //成功拆一个包
// }



// // ==============================================
// // 函数：ET模式循环读取数据 → 存入用户层缓冲区
// // 作用：一次把内核缓冲区数据读空
// // ==============================================
// static bool readToBuffer(int fd){
//     // 从全局哈希表找到当前连接
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()){
//         return false; // 找不到，直接返回
//     }

//     // 临时数组：接收read读取的数据
//     char tmp[BUF_SIZE];


//     // ET模式必须循环读，直到返回EAGAIN
//     while(1){
//         // 从socket读取数据到tmp数组
//         ssize_t n = read(fd, tmp, BUF_SIZE);

//         if(n > 0){
//             // 读到数据 → 追加到连接的读缓冲区
//             it->second.read_buf.insert(
//                 it->second.read_buf.end(),
//                 tmp,
//                 tmp + n
//             );
//         }
//         else if(n == 0){
//             // read返回0 → 客户端关闭连接
//             return false;
//         }
//         else{
//             // n < 0 出错
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                  // 没有数据了 → 正常退出循环
//                  break;
//             }
//             // 其他错误 → 关闭连接
//             return false;
//         }
//     }

//      // ----------------------
//     // 数据读完 → 循环拆包（可能一次收到多个包：粘包）
//     // ----------------------
//     while (parseOnePacket(it->second));
//     return true;
// }

// // ==============================================
// // 函数：关闭并清理一个客户端连接
// // ==============================================
// static void closeConnection(int epoll_fd, int fd){
//     // 1. 从epoll中移除监听
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
//     // 2. 关闭socket文件描述符
//     close(fd);
//     // 3. 从全局连接表删除
//     g_connections.erase(fd);
// }

// // ==============================================
// // 主函数：启动服务器
// // ==============================================
// void runServer(uint16_t ports){
//     // ----------------------
//     // 1. 创建TCP socket
//     // ----------------------
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);

//     if(sockfd < 0){
//         perror("socket创建失败");
//         return;
//     }

//     // ----------------------
//     // 2. 设置端口复用（防止重启报错address in use）
//     // ----------------------
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     // ----------------------
//     // 3. 绑定IP和端口
//     // ----------------------
//     struct sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
//         perror("bind绑定失败");
//         close(sockfd);
//         return;
//     }

//     // ----------------------
//     // 4. 开始监听
//     // ----------------------
//     if(listen(sockfd, 100) < 0){
//         perror("listen监听失败");
//         close(sockfd);
//         return;
//     }

//     // ----------------------
//     // 5. 创建epoll实例
//     // ----------------------
//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("创建失败");
//         close(sockfd);
//         return;
//     }

//     // ----------------------
//     // 6. 将监听socket加入epoll
//     // ----------------------
//     epollAddFd(epoll_fd, sockfd);


//     // 存储epoll返回的事件
//     struct epoll_event events[MAX_EVENTS];
//     printf("【工业级拆包服务器】启动成功 端口:%d\n", ports);

//     // ----------------------
//     // 7. 主线程死循环：epoll事件驱动
//     // ----------------------

//     while(1){
//         // 等待事件（-1=永久阻塞）
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;

//         //便利所有就绪事件
//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;

//             if(fd == sockfd){
//                 // ----------------------
//                 // 事件1：有新客户端连接
//                 // ----------------------
//                 while(1){
//                     sockaddr_in cli_addr;
//                     socklen_t cli_len = sizeof(cli_addr);

//                     //接收连接
//                     int cfd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);
//                     if(cfd < 0){
//                         if(errno == EAGAIN) break;  //没有更多连接
//                         continue;
//                     }
//                     // 新建连接对象
//                     Connection conn;
//                     conn.fd = cfd;

//                     //加入全局表
//                     g_connections[cfd] = conn;
//                     //加入epoll监听
//                     epollAddFd(epoll_fd, cfd);
//                 }
//             }
//             else{
//                 // ----------------------
//                 // 事件2：客户端发来数据 / 断开连接
//                 // ----------------------
//                 bool ok = readToBuffer(fd);
//                 if(!ok){
//                     // 读取失败/断开 → 关闭连接
//                     closeConnection(epoll_fd, fd);
//                 }
//             }
//         }
//     }
//     // ----------------------
//     // 理论上永远不会走到这里（while死循环）
//     // ----------------------
//     close(epoll_fd);
//     close(sockfd);
// }


// ==============================================
// 4.0 异步写缓冲区 + EPOLLOUT 完整版
// 新特性：用户态写队列 + 异步发送 + 永不丢包 std::vector<char> write_buf; // 写缓冲区
// ==============================================
// #include <stdio.h>//输出输入：printf打印日志、perror打印系统错误
// #include <stdlib.h> // 通用工具：内存分配、退出
// #include <string.h> // 内存操作：memset、strlen
// #include <unistd.h> // Unix系统调用：close、read、write
// // // Linux网络编程核心头文件
// #include <sys/socket.h> // socket、bind、listen、accept、send、recv
// #include <netinet/in.h> // sockaddr_in结构体、htons/ntohl字节序转换
// #include <fcntl.h> // fcntl：设置非阻塞模式
// #include <errno.h> // 错误码：EAGAIN、EWOULDBLOCK等
// #include <sys/epoll.h>// epoll_create1、epoll_ctl、epoll_wait
// // C++容器
// #include <unordered_map> // 哈希表：快速查找客户端连接
// #include "server.h"

// // 宏定义
// #define MAX_EVENTS 1024   // epoll_wait最多接收多少个事件
// #define BUF_SIZE 1024   // 每次read读取的临时缓冲区大小
// #define MAX_PACKET_SIZE 65536 // 最大包长度64KB，防止攻击

// // 全局哈希表：保存所有客户端连接
// // key：客户端fd
// // value：Connection结构体（包含读写缓冲区）
// static std::unordered_map<int,Connection>g_connections;

// // ==============================================
// // 函数：设置文件描述符为非阻塞模式
// // 作用：ET模式必须用非阻塞，否则会卡死
// // ==============================================
// int setnonblocking(int fd){
//     int old_flag = fcntl(fd, F_GETFL);
//     int new_flag = old_flag | O_NONBLOCK;
//     fcntl(fd, F_SETFL, new_flag);
//     return old_flag;
// }


// // ==============================================
// // 函数：修改epoll监听的事件（新增/移除EPOLLOUT）
// // 作用：动态控制监听读、写事件
// // ==============================================
// void epoll_mod(int epoll_fd, int fd, int events){
//     epoll_event ev;
//     ev.events = events; // 要监听的事件（EPOLLIN/EPOLLOUT）
//     ev.data.fd = fd;    // 关联的fd
//     // 调用epoll_ctl，MOD表示修改已有fd的监听事件
//     epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
// }

// // ==============================================
// // 函数：将fd添加到epoll监听
// // 初始只监听读事件 EPOLLIN
// // ==============================================
// void epollAddFd(int epoll_fd, int fd){
//     epoll_event ev;
//     // ET边缘触发 + 监听可读
//     ev.events = EPOLLIN | EPOLLET;
//     ev.data.fd = fd;
//     // 添加到epoll
//     epoll_ctl(epoll_fd,  EPOLL_CTL_ADD, fd, &ev);
//     //设置非阻塞
//     setnonblocking(fd);
// }

// // ==============================================
// // 协议头：1字节对齐，固定4字节长度
// // ==============================================
// #pragma pack(push, 1)
// struct PacketHeader{
//     uint32_t data_len;   // 数据体长度（必须网络字节序）
// };
// #pragma pack(pop)

// // ==============================================
// // 🔥 核心函数：异步发送（真正发送数据）
// // 功能：循环把write_buf里的数据发给内核，直到发完或EAGAIN
// // ==============================================
// bool async_write(int epoll_fd, Connection& conn){
//     // 循环：只要写缓冲区不为空，就一直尝试发送
//     while(!conn.write_buf.empty()){
//         // 调用write：尝试把write_buf里所有数据写入内核
//         ssize_t n = write(
//             conn.fd,    // 客户端fd
//             conn.write_buf.data(),   // 发送队列首地址
//             conn.write_buf.size()   // 队列数据长度
//         );
//         // --------------------
//         // 情况1：成功发送了n字节
//         // --------------------
//         if(n > 0){
//             // 从队列头部删除已发送的n字节（FIFO队列）
//             conn.write_buf.erase(
//                 conn.write_buf.begin(),
//                 conn.write_buf.begin() + n
//             );
//         }
//          // --------------------
//         // 情况2：发送失败
//         // --------------------
//         else{
//             // errno == EAGAIN：内核发送缓冲区满了！
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 // ------------------------------
//                 // 🔥 关键：开启EPOLLOUT监听
//                 // 告诉内核：等你有空了，通知我！
//                 // ------------------------------
//                 epoll_mod(epoll_fd, conn.fd, EPOLLIN | EPOLLET | EPOLLOUT);
//                 return true;    // 不报错，只是暂时发不了
//             }

//             // 其他错误：连接断开、出错等
//             return false;
//         }

//     }
//     // --------------------
//     // 走到这里：write_buf已经全部发空了
//     // --------------------
//     // 关闭EPOLLOUT！！！
//     // 因为只要缓冲区可写，EPOLLOUT会一直触发，不关掉CPU 100%
//     epoll_mod(epoll_fd, conn.fd, EPOLLIN | EPOLLET);
//     return true;
// }

// // ==============================================
// // 函数：将数据加入发送队列（不直接发送）
// // ==============================================
// bool add_write_queue(int epoll_fd, Connection& conn, const char* data, int len){
//     // 把要发送的数据，追加到write_buf尾部（入队）
//     conn.write_buf.insert(
//         conn.write_buf.end(),
//         data,
//         data + len
//     );
//     // 入队后，立即尝试发送
//     return async_write(epoll_fd, conn);
// }

// // ==============================================
// // 函数：拆一个完整的包（和之前一样，只是发送改为异步）
// // ==============================================
// static bool parseOnePacket(int epoll_fd, Connection& conn){
//     // 不够读包头，返回
//     if(conn.read_buf.size() < sizeof(PacketHeader)){
//         return false;
//     }

//     // 强转包头
//     PacketHeader* header = (PacketHeader*)conn.read_buf.data();
//     // 网络字节序转主机字节序
//     uint32_t data_len = ntohl(header->data_len);


//     // 非法包检查
//     if (data_len == 0 || data_len > MAX_PACKET_SIZE) {
//         printf("[错误] 非法包长度\n");
//         return false;
//     }

//     // 总长度 = 包头4字节 + 数据长度
//     uint32_t total_len = sizeof(PacketHeader) + data_len;
//     // 不够一个完整包，返回
//     if(conn.read_buf.size() < total_len)
//         return false;

//     // 数据指针：跳过包头
//     char *data_ptr = conn.read_buf.data() + sizeof(PacketHeader);


//     // ======================
//     // 🔥 关键修改：不再直接write
//     // 改为：加入发送队列，异步发送
//     // ======================
//     if(!add_write_queue(epoll_fd, conn, data_ptr, data_len)){
//         return false;
//     }

//     printf("[成功拆包&异步发送] fd=%d 长度=%u\n", conn.fd, data_len);

//     // 移除已处理的数据包
//     conn.read_buf.erase(
//         conn.read_buf.begin(),
//         conn.read_buf.begin() + total_len
//     );
//     return true;
// }


// // ==============================================
// // 函数：ET模式循环读取数据到读缓冲区
// // ==============================================
// static bool readToBuffer(int epoll_fd, int fd){
//     // 找到当前连接
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()) return false;

//     //临时数组
//     char tmp[BUF_SIZE];

//     //循环读，直到EAGAIN
//     while(1){
//         ssize_t n = read(fd, tmp, BUF_SIZE);
//         if(n > 0){
//             // 读到数据，追加到read_buf
//             it->second.read_buf.insert(it->second.read_buf.end(), tmp, tmp + n);
//         }else if(n == 0){
//             //客户端关闭连接
//             return false;
//         }else{
//             //无数据了，退出循环
//             if(errno == EAGAIN || errno == EWOULDBLOCK) break;
//             //其他错误
//             return false;
//         }
//     }

//     //循环拆包（处理粘包）
//     while(parseOnePacket(epoll_fd, it->second));
//     return true;
// }

// // ==============================================
// // 函数：关闭连接并清理资源
// // ==============================================
// static void closeConnection(int epoll_fd, int fd){
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
//     close(fd);
//     g_connections.erase(fd);
// }

// // ==============================================
// // 主函数：服务器启动入口
// // ==============================================
// void runServer(uint16_t ports){
//     // 1. 创建TCP socket
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("socket"); return;
//     }

//     // 2. 设置端口复用
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));


//     // 3. 绑定IP和端口
//     sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0){
//         perror("bind"); close(sockfd); return;
//     }

//     // 4. 监听
//     if(listen(sockfd, 100) < 0){
//         perror("listen"); close(sockfd); return;
//     }

//     // 5. 创建epoll
//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("epoll"); close(sockfd); return;
//     }

//     // 6. 监听socket加入epoll
//     epollAddFd(epoll_fd, sockfd);

//     epoll_event events[MAX_EVENTS];
//     printf("【异步发送服务器】启动成功 port:%d\n", ports);

//     // 7. 主线程事件循环
//     while(1){
//         // 等待事件（阻塞）
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         if(nready <= 0) continue;

//         //遍历所有就绪事件
//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;
//             int ev = events[i].events;  //事件类型(读/写)

//             // --------------------
//             // 事件1：新客户端连接
//             // --------------------
//             if(fd == sockfd){
//                 while(1){
//                     sockaddr_in cli_addr;
//                     socklen_t cli_len = sizeof(cli_addr);
//                     int cfd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);
//                     if(cfd < 0){
//                         if(errno == EAGAIN) break;
//                         continue;
//                     }
//                     // 新建连接，加入全局表和epoll
//                     Connection conn;
//                     conn.fd = cfd;
//                     g_connections[cfd] = conn;
//                     epollAddFd(epoll_fd, cfd);
//                 }
//             }
//             // --------------------
//             // 事件2：已连接客户端（读/写事件）
//             // --------------------
//             else{
//                 bool ok = true;

//                 // 🔥 处理可写事件 EPOLLOUT
//                 if(ev & EPOLLOUT){
//                     // 内核通知：可以发送数据了！
//                     ok = async_write(epoll_fd, g_connections[fd]);
//                 }

//                 //处理可读事件EPOLLIN
//                 if(ok&& (ev & EPOLLIN)){
//                     ok = readToBuffer(epoll_fd, fd); 
//                 }

//                 //出错或断开，关闭连接
//                 if(!ok){
//                     closeConnection(epoll_fd, fd);
//                 }
//             }
//         }
//     }

//     //永远不会走到这里
//     close(epoll_fd);
//     close(sockfd);
// }

// ==============================================
// 5.0 长连接 + 心跳机制 + 超时管理 纯高并发框架
// 无业务、无HTTP、纯底层、企业级可用
// ==============================================

// #include <stdio.h>  //输出输入：printf打印日志、perror打印系统错误
// #include <stdlib.h> // 通用工具：内存分配、退出
// #include <string.h> // 内存操作：memset、strlen
// #include <unistd.h> // Unix系统调用：close、read、write
// // Linux网络编程核心头文件
// #include <sys/socket.h>     // socket、bind、listen、accept、send、recv
// #include <netinet/in.h>     // sockaddr_in结构体、htons/ntohl字节序转换
// #include <fcntl.h>      //  fcntl：设置非阻塞模式
// #include <errno.h>      // 错误码：EAGAIN、EWOULDBLOCK等
// #include <sys/epoll.h>  // epoll_create1、epoll_ctl、epoll_wait
// // C++容器
// #include <unordered_map>    // 哈希表：快速查找客户端连接
// #include <time.h>   // 时间函数
// #include "server.h"

// // ===================== 宏定义 =====================
// #define MAX_EVENTS 1024     // epoll_wait 一次最多处理多少事件
// #define BUF_SIZE 1024   // 每次 read 读取的临时缓冲区大小
// #define MAX_PACKET_SIZE 65536   // 最大包长度，防止恶意发包攻击

// // 超时配置（秒）
// #define IDLE_TIMEOUT 15 // 15秒没有任何活动 → 判定超时踢出
// #define CHECK_INTERVAL 3    // 每3秒检查一次有没有超时连接

// // ===================== 全局变量 =====================
// // 哈希表：保存所有客户端连接
// // key = 文件描述符 fd
// // value = Connection 结构体（包含读写缓冲区、最后活跃时间）
// static std::unordered_map<int, Connection>g_connections;

// // 上次检查超时的时间，避免频繁检查
// static time_t last_check_time = 0;


// // ===================== 设置非阻塞 =====================
// // 作用：把 socket 变成“非阻塞模式”
// // 非阻塞 = 读不到/发不出 不会卡住，直接返回 EAGAIN
// int setnonblocking(int fd){
//     int old_flag = fcntl(fd, F_GETFL);  // 获取当前 fd 的状态标记
//     int new_flag = old_flag | O_NONBLOCK;   // 添加“非阻塞”标记
//     fcntl(fd, F_SETFL, new_flag);   // 设置新标记
//     return old_flag;    // 返回旧标记（备用）
// }

// // ===================== 修改 epoll 监听事件 =====================
// // 作用：动态给某个 fd 增加/移除监听事件（比如加 EPOLLOUT）
// void epoll_mod(int epoll_fd, int fd, int events){
//     epoll_event ev;
//     ev.events = events; // 要监听的事件：EPOLLIN / EPOLLOUT / EPOLLET
//     ev.data.fd = fd;    // 关联哪个客户端 fd
//     epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);    // 修改 epoll 监听
// }

// // ===================== 添加 fd 到 epoll 监听 =====================
// // 作用：新客户端连接上来，把它加入 epoll 监听
// void epollAddFd(int epoll_fd, int fd){
//     epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;  // 初始只监听“读事件”+ ET 模式
//     ev.data.fd = fd;
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);    // 添加到 epoll
//     setnonblocking(fd); // 必须设置非阻塞！
// }

// // ===================== 协议头（固定4字节） =====================
// // #pragma pack(1) = 强制1字节对齐
// // 作用：不让编译器乱加填充字节，保证包头大小永远 = 4 字节
// #pragma pack(push, 1)
// struct PacketHeader{
//     uint32_t data_len;  //数据体长度(网络字节序)
// };
// #pragma pack(pop)

// // ===================== 异步发送核心函数 =====================
// // 功能：循环把 write_buf 队列里的数据发给客户端
// // 发不完 → 等 EPOLLOUT
// bool async_write(int epoll_fd, Connection& conn){
//     // 只要写缓冲区不为空，就一直尝试发送
//     while(!conn.write_buf.empty()){
//             // 调用 write：把缓冲区数据往内核里写
//             ssize_t n = write(conn.fd, conn.write_buf.data(), conn.write_buf.size());

//         // =====================
//         // 情况1：发送成功 n 字节
//         // =====================
//         if(n > 0){
//             // 删除已发送的前 n 字节（队列 FIFO）
//             conn.write_buf.erase(conn.write_buf.begin(), conn.write_buf.begin() + n);
//             // 刷新最后活跃时间（发了消息 = 活着）
//             conn.last_active_time = time(nullptr);
//         }
//         // =====================
//         // 情况2：发送失败
//         // =====================
//         else{
//             // EAGAIN / EWOULDBLOCK = 内核发送缓冲区满了！
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 // 🔥 开启 EPOLLOUT：等内核有空再通知我
//                 epoll_mod(epoll_fd, conn.fd, EPOLLIN | EPOLLET | EPOLLOUT);
//                 return true;    // 暂时发不了，不是错误
//             }

//             // 真正错误（客户端断开）
//             return false;
//         }
//     }
//     // =====================
//     // 走到这里 = 全部发完了
//     // =====================
//     // 关闭 EPOLLOUT，避免一直触发
//     epoll_mod(epoll_fd, conn.fd, EPOLLIN | EPOLLET);
//     return true;
// }

// // ===================== 把数据加入发送队列 =====================
// // 作用：不直接发送，先入队，保证不丢包
// bool add_write_queue(int epoll_fd, Connection& conn, const char*data, int len){
//     //数据追加到write_buf 尾部
//     conn.write_buf.insert(conn.write_buf.end(), data, data + len);
//     // 入队后立即尝试发送
//     return async_write(epoll_fd, conn);
// }

// // ===================== 关闭连接 =====================
// // 作用：清理资源，从 epoll 移除，关闭 fd
// static void closeConnection(int epoll_fd, int fd){
//     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);    //从epoll删除
//     close(fd);  //关闭socket
//     g_connections.erase(fd);    //从全局表删除
//     printf("连接关闭: fd=%d\n", fd);
// }

// // ===================== 检查超时连接（核心） =====================
// // 功能：遍历所有客户端，超过15秒没活动 → 踢掉
// void check_idle_connections(int epoll_fd){
//     time_t now = time(nullptr); //当前时间

//     //没到检查时间、直接返回(每3秒检查一次)
//     if(now - last_check_time < CHECK_INTERVAL)
//         return;
    
//     //更新最后检查时间
//     last_check_time = now;

//     //遍历所有连接
//     auto it = g_connections.begin();
//     while(it != g_connections.end()){
//         // 判断：当前时间 - 最后活跃时间 > 15秒
//         if(now - it->second.last_active_time > IDLE_TIMEOUT){
//             printf("连接超时踢出: fd=%d\n", it->first);
//             closeConnection(epoll_fd, it->first);       // 关闭超时连接
//         }
//         ++it;   //继续下一个检查
//     }
// }

// // ===================== 拆包函数（心跳+普通消息） =====================
// // 功能：从 read_buf 里拆出一个完整的数据包
// static bool parseOnePacket(int epoll_fd, Connection& conn){
//     //缓冲区不够读包头(4字节), 返回false
//     if(conn.read_buf.size() < sizeof(PacketHeader))
//         return false;
    
//     //把缓冲区前4个字节强转成包头结构体
//     PacketHeader* header = (PacketHeader*)conn.read_buf.data();
//     //网络字节序->本机字节序
//     uint32_t data_len = ntohl(header->data_len);

//     // 非法包：长度为0 或 太长
//     if(data_len == 0 || data_len > MAX_PACKET_SIZE)
//         return false;

//     // 一个完整包总长度 = 包头4字节 + 数据长度
//     uint32_t total_len = sizeof(PacketHeader) + data_len;

//     //缓冲区不够一个完整包,返回
//     if(conn.read_buf.size() < total_len)
//         return false;

//     //数据指针 = 包头之后的位置
//     char * data_ptr = conn.read_buf.data() + sizeof(PacketHeader);

//     // =====================
//     // 心跳包判断
//     // 长度=9，内容=heartbeat
//     // =====================
//     if(data_len == 9 && memcmp(data_ptr,"heartbeat", 9) == 0){
//         printf("fd=%d 心跳包\n", conn.fd);
//         conn.last_active_time = time(nullptr);  //刷新活跃时间
//     }
//     // =====================
//     // 普通消息：回显
//     // =====================
//     else{
//         add_write_queue(epoll_fd, conn, data_ptr, data_len);
//     }


//     //把已处理的包从缓冲区删除
//     conn.read_buf.erase(conn.read_buf.begin(), conn.read_buf.begin() + total_len);
//     return true;
// }

// // ===================== 读取客户端数据 =====================
// // ET模式：循环读，直到 EAGAIN
// static bool readToBuffer(int epoll_fd, int fd){
//     // 找到这个客户端的连接信息
//     auto it = g_connections.find(fd);
//     if(it == g_connections.end()) return false;

//     //临时缓冲区
//     char tmp[BUF_SIZE];

//     //循环读取
//     while(1){
//         ssize_t n = read(fd, tmp, BUF_SIZE);

//         // =====================
//         // 读到数据
//         // =====================
//         if(n > 0){
//             //数据追加到读缓冲区
//             it->second.read_buf.insert(it->second.read_buf.end(), tmp, tmp + n);
//             //刷新活跃时间(收到消息 = 活着)
//             it->second.last_active_time = time(nullptr);
//         }
//         // =====================
//         // 客户端主动关闭
//         // =====================
//         else if(n == 0){
//             return false;
//         }
//         // =====================
//         // 读空了（EAGAIN）或错误
//         // =====================
//         else{
//             if(errno == EAGAIN || errno == EWOULDBLOCK)
//                 break;  // 数据读完了
//             return false;       //真正错误
//         }
//     }

//     //循环拆包(处理粘包)
//     while(parseOnePacket(epoll_fd, it->second));
//     return true;

// }

// // ===================== 服务器主函数 =====================

// void runServer(uint16_t ports){
//     // 1. 创建 TCP socket
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0){
//         perror("socket");
//         return;
//     }

//     //2.设置端口复用(重启不报错)
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     //3.绑定ip + 端口
//     sockaddr_in addr;
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(ports);

//     if(bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0){
//         perror("bind");
//         close(sockfd);
//         return ;
//     }

//     //4.监听终端
//     if(listen(sockfd, 1024) < 0){
//         perror("listen");
//         close(sockfd);
//         return ;
//     }

//     //5.创建epoll实例
//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0){
//         perror("listen");
//         close(sockfd);
//         return;
//     }

//     //6.把监听socket加入epoll
//     epollAddFd(epoll_fd, sockfd);

//     // 事件数组，接收 epoll 回来的事件
//     epoll_event events[MAX_EVENTS];
//     last_check_time = time(nullptr);    //初始化超时检查时间

//     printf("【长连接+心跳+超时】服务器启动 port:%d\n", ports);


//     // =====================
//     // 主线程死循环：事件驱动
//     // =====================
//     while(1){
//         // epoll_wait 等待事件
//         // 超时 1000ms = 1秒
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

//         //每次循环都检查超时连接
//         check_idle_connections(epoll_fd);

//         if(nready <= 0) continue;

//         //遍历所有就绪事件
//         for(int i = 0; i < nready; i++){
//             int fd = events[i].data.fd;
//             int ev = events[i].events;

//             // =====================
//             // 事件1：新客户端连接
//             // =====================
//             if(fd == sockfd){
//                 while(1){
//                     struct sockaddr_in cli_addr;
//                     socklen_t cli_len = sizeof(cli_addr);
//                     int cfd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);

//                     if(cfd < 0){
//                         if(errno == EAGAIN) break;
//                         continue;
//                     }

//                     //新建连接结构体
//                     Connection conn;
//                     conn.fd = cfd;
//                     conn.last_active_time = time(nullptr);  //初始化活跃时间

//                     //加入全局表+ epoll
//                     g_connections[cfd] = conn;
//                     epollAddFd(epoll_fd, cfd);
//                     printf("新连接: fd=%d\n", cfd);
//                 }
//             }
//             // =====================
//             // 事件2：已连接客户端（读/写事件）
//             // =====================
//             else{
//                 bool ok = true;

//                 // 可写事件：内核有空，可以发送数据
//                 if(ev & EPOLLOUT){
//                     ok = async_write(epoll_fd, g_connections[fd]);
//                 }

//                 // 可读事件：客户端发消息来了
//                 if(ok && (ev & EPOLLIN)){
//                     ok = readToBuffer(epoll_fd, fd);
//                 }

//                 //出错或断开->关闭连接
//                 if(!ok){
//                     closeConnection(epoll_fd, fd);
//                 }
//             }
//         }
//     }

//     //不会执行到这里
//     close(epoll_fd);
//     close(sockfd);
// }


// // ==============================================
// // 6.0 版本：主 Reactor + 线程池分发
// // 主线程只负责 accept 新连接，然后分发给 Worker 线程池
// // 客户端连接的读写处理全部由 Worker 完成
// // ==============================================

// #include <stdio.h>          // printf, perror
// #include <stdlib.h>         // exit
// #include <string.h>         // memset, memcmp
// #include <unistd.h>         // close, read, write
// #include <fcntl.h>          // fcntl: F_GETFL, F_SETFL, O_NONBLOCK
// #include <errno.h>          // errno, EAGAIN, EWOULDBLOCK
// #include <sys/socket.h>     // socket, bind, listen, accept, setsockopt
// #include <netinet/in.h>     // sockaddr_in, AF_INET, INADDR_ANY, htons
// #include <sys/epoll.h>      // epoll_create1, epoll_ctl, epoll_wait, epoll_event

// #include "server.h"         // 引入宏定义：MAX_EVENTS, BUF_SIZE 等
// #include "threadpool.h"     // ThreadPool 类
// #include "connection.h"     // Connection 结构体

// // 全局线程池指针（供主线程使用）
// static ThreadPool* g_thread_pool = nullptr;

// // ==================== 设置非阻塞 ====================
// // 通用工具函数，被 5.0 和 6.0 版本共用
// int setnonblocking(int fd)
// {
//     // 获取当前 fd 的状态标记
//     int old_flag = fcntl(fd, F_GETFL);
//     // 在原状态基础上添加非阻塞标记
//     int new_flag = old_flag | O_NONBLOCK;
//     // 设置新状态
//     fcntl(fd, F_SETFL, new_flag);
//     // 返回旧标记（方便恢复原状态时使用）
//     return old_flag;
// }

// // ==================== 添加 fd 到 epoll ====================
// // 通用工具函数，被 5.0 和 6.0 版本共用
// void epollAddFd(int epoll_fd, int fd)
// {
//     // 定义 epoll 事件结构体
//     epoll_event ev;
//     // 设置监听可读事件 + ET 边缘触发模式
//     ev.events = EPOLLIN | EPOLLET;
//     // 存储 fd 标识（epoll 触发事件时会返回这个值）
//     ev.data.fd = fd;
//     // 调用 epoll_ctl 将 fd 添加到 epoll 监听列表
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
//     // 设置非阻塞（ET 模式必须用非阻塞 socket）
//     setnonblocking(fd);
// }

// // ==================== 6.0 主服务器函数 ====================
// void runServer6_0(uint16_t ports)
// {
//     // ====================
//     // 第1步：创建 TCP 监听 socket
//     // ====================
//     int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//     if(sockfd < 0)
//     {
//         perror("socket 创建失败");
//         return;
//     }

//     // 设置端口复用（服务器重启后可以立刻绑定端口）
//     int opt = 1;
//     setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     // ====================
//     // 第2步：绑定 IP + 端口
//     // ====================
//     sockaddr_in addr;
//     addr.sin_family = AF_INET;   // IPv4
//     addr.sin_addr.s_addr = INADDR_ANY;  // 绑定所有网卡
//     addr.sin_port = htons(ports);   // 端口转网络字节序
    
//     if(bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
//     {
//         perror("bind 绑定失败");
//         close(sockfd);
//         return;
//     }

//     // ====================
//     // 第3步：开始监听
//     // ====================
//     if(listen(sockfd, 1024) < 0)
//     {
//         perror("listen 监听失败");
//         close(sockfd);
//         return;
//     }

//     // ====================
//     // 第4步：创建主线程的 epoll
//     // 主线程的 epoll 只监听 listen fd
//     // ====================
//     int epoll_fd = epoll_create1(0);
//     if(epoll_fd < 0)
//     {
//         perror("epoll_create1 失败");
//         close(sockfd);
//         return;
//     }

//     // ====================
//     // 第5步：将监听 socket 加入 epoll
//     // ====================
//     epoll_event listen_ev;
//     listen_ev.events = EPOLLIN | EPOLLET;   // 只读 + ET 模式
//     listen_ev.data.fd = sockfd;
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &listen_ev);

//     // ====================
//     // 第6步：创建并启动线程池
//     // ====================
//     g_thread_pool = new ThreadPool(4);  // 创建 4 个 Worker
//     g_thread_pool->start();     // 启动所有 Worker

//     // ====================
//     // 第7步：主事件循环（Reactor 模式）
//     // 主线程只做三件事：
//     //   1. 等待新连接
//     //   2. accept 接收新连接
//     //   3. 将连接分发给 Worker
//     // ====================
//     epoll_event events[MAX_EVENTS];
//     printf("\n");
//     printf("========================================\n");
//     printf("【6.0 多线程版本】服务器启动\n");
//     printf("端口: %d\n", ports);
//     printf("Worker 数量: 4\n");
//     printf("架构: 主 Reactor + Worker 线程池\n");
//     printf("========================================\n\n");

//     while(1){
//         // 阻塞等待事件（-1 表示无限等待）
//         int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

//         if(nready < 0) continue;


//         // 遍历所有就绪事件
//         // 在 6.0 版本中，主线程只会收到 listen fd 的事件
//         // 客户端连接的事件全部由 Worker 处理
//         for(int i = 0; i < nready; i++)
//         {
//             int fd = events[i].data.fd;

//             //只有listen fd会触发事件
//             if(fd == sockfd)
//             {
//                 // ====================
//                 // ET 模式必须循环 accept
//                 // 直到返回 EAGAIN 为止
//                 // ====================
//                 while(1){
//                     sockaddr_in cli_addr;   // 客户端地址
//                     socklen_t cli_len = sizeof(cli_addr);

//                     // 接受一个新连接
//                     int cfd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);

//                     if(cfd< 0)
//                     {
//                         if(errno == EAGAIN){
//                             break;  //没有更多连接了，正常退出
//                         }
//                         //其他错误，继续尝试（不break）
//                         continue;
//                     }

//                     //新连接成功
//                     printf("accept新连接: fd=%d\n", cfd);

//                     // ====================
//                     // 关键步骤：分发给 Worker
//                     // ====================
//                     // 1. 主线程设置非阻塞
//                     setnonblocking(cfd);

//                     // 2. 通过线程池分发给某个 Worker
//                     // Worker 会自己设置 epoll 监听
//                     g_thread_pool->distributeConnection(cfd);
//                 }
//             }
//         }
//     }

//     // ====================
//     // 清理资源（理论上不会走到这里）
//     // ====================
//     printf("\n服务器关闭...\n");
//     g_thread_pool->stop();  // 停止线程池
//     delete g_thread_pool;   // 释放线程池内存
//     g_thread_pool = nullptr;
//     close(epoll_fd);    // 关闭 epoll
//     close(sockfd);  // 关闭监听 socket
// }








// ==============================================
// 7.0 版本：主 Reactor + 线程池分发
// 主线程只负责 accept 新连接，然后分发给 Worker 线程池
// 客户端连接的读写处理全部由 Worker 完成
// 新增：添加日志功能
// ==============================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "server.h"
#include "threadpool.h"
#include "connection.h"
#include "config.h"         // 🆕 9.2：配置中心
#include "logger.h"       // 🆕 引入日志系统
#include "signal_handler.h" // 🆕 8.0：信号处理 + 优雅关闭标志

// 全局线程池指针（供主线程使用）
static ThreadPool* g_thread_pool = nullptr;

// ==================== 设置非阻塞 ====================
// 通用工具函数，被 5.0 和 6.0 版本共用
int setnonblocking(int fd)
{
    // 获取当前 fd 的状态标记
    int old_flag = fcntl(fd, F_GETFL);
    // 在原状态基础上添加非阻塞标记
    int new_flag = old_flag | O_NONBLOCK;
    // 设置新状态
    fcntl(fd, F_SETFL, new_flag);
    // 返回旧标记（方便恢复原状态时使用）
    return old_flag;
}

// ==================== 添加 fd 到 epoll ====================
// 通用工具函数，被 5.0 和 6.0 版本共用
void epollAddFd(int epoll_fd, int fd)
{
    // 定义 epoll 事件结构体
    epoll_event ev;
    // 设置监听可读事件 + ET 边缘触发模式
    ev.events = EPOLLIN | EPOLLET;
    // 存储 fd 标识（epoll 触发事件时会返回这个值）
    ev.data.fd = fd;
    // 调用 epoll_ctl 将 fd 添加到 epoll 监听列表
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    // 设置非阻塞（ET 模式必须用非阻塞 socket）
    setnonblocking(fd);
}

// ==================== 6.0 主服务器函数 ====================
void runServer6_0(uint16_t ports)
{
    // ====================
    // 第1步：创建 TCP 监听 socket
    // ====================
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("socket 创建失败");
        return;
    }

    // 设置端口复用（服务器重启后可以立刻绑定端口）
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    // ====================
    // 第2步：绑定 IP + 端口
    // ====================
    sockaddr_in addr;
    addr.sin_family = AF_INET;   // IPv4
    addr.sin_addr.s_addr = INADDR_ANY;  // 绑定所有网卡
    addr.sin_port = htons(ports);   // 端口转网络字节序
    
    if(bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind 绑定失败");
        close(sockfd);
        return;
    }

    // ====================
    // 第3步：开始监听（9.2 改造：backlog 从配置文件读，默认 1024）
    // ====================
    int backlog = Config::instance().getInt("server.backlog", 1024);
    if(listen(sockfd, backlog) < 0)
    {
        perror("listen 监听失败");
        close(sockfd);
        return;
    }

    // ====================
    // 第4步：创建主线程的 epoll
    // 主线程的 epoll 只监听 listen fd
    // ====================
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0)
    {
        perror("epoll_create1 失败");
        close(sockfd);
        return;
    }

    // ====================
    // 第5步：将监听 socket 加入 epoll
    // ====================
    epoll_event listen_ev;
    listen_ev.events = EPOLLIN | EPOLLET;   // 只读 + ET 模式
    listen_ev.data.fd = sockfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &listen_ev);

    // ====================
    // 第6步：创建并启动线程池（9.2 改造：线程数从配置文件读，默认4）
    // ====================
    int threads = Config::instance().getInt("performance.threads", 4);
    g_thread_pool = new ThreadPool(threads);  // 创建 N 个 Worker（N 来自配置，默认4）
    g_thread_pool->start();     // 启动所有 Worker

    // 🆕 10.0：局部时间轮已在 Worker::start() 中初始化
    //   超时回调由每个 Worker 的 loop() 中的 tick() 直接驱动
    LOG_INFO("Worker 线程池已启动，局部时间轮（10.0 版）已就绪");

    // ====================
    // 第7步：主事件循环（Reactor 模式）
    // 主线程只做三件事：
    //   1. 等待新连接
    //   2. accept 接收新连接
    //   3. 将连接分发给 Worker
    // 🟢 8.0 改造：
    //   - 循环条件：检测 SignalHandler::isShutdownRequested()
    //   - epoll_wait 超时由 -1 改为 500ms
    //     好处1：即使没有任何网络事件，每 500ms 也会醒来一次检查退出标志
    //     好处2：Ctrl+C 发 SIGINT 会打断 epoll_wait（返回-1且errno=EINTR），马上能检测到标志
    // ====================
    epoll_event events[MAX_EVENTS];
    LOG_INFO("========================================");
    LOG_INFO("【9.0 连接池版】服务器启动");
    LOG_INFO("端口: %d", ports);
    LOG_INFO("Worker 数量: %d", threads);
    LOG_INFO("架构: 主 Reactor + Worker 线程池（每个 Worker 自带分片连接池）");
    LOG_INFO("优雅关闭: 按 Ctrl+C 或 kill -15 触发");
    LOG_INFO("连接池: 每个 Worker 预分配 %d 个 Connection（buffer reserve=%d B），从池 acquire/release",
             Config::instance().getInt("server.max_connections", 1000) / (threads > 0 ? threads : 1),
             Config::instance().getInt("pool.reserve_bytes", 8192));
    LOG_INFO("========================================");

    // 🟢 8.0【关键】解除主线程的信号阻塞
    //   init() 中已阻塞 SIGINT/SIGTERM（Worker 线程继承阻塞，无法接收信号）
    //   这里解除主线程阻塞 → 主线程成为唯一能接收退出信号的线程
    //   这样 Ctrl+C/kill 会立刻打断主线程的 epoll_wait()，触发优雅关闭
    SignalHandler::allowSignals();

    // 🟢 8.0：只要没请求关闭就继续循环
    while(!SignalHandler::isShutdownRequested()){
        // 🟢 8.0：超时改为 500ms，不再死等 -1
        //   - 有事件 → 立刻返回处理
        //   - 无事件 → 500ms 后自动醒来检查 isShutdownRequested
        //   - 被信号打断 → 返回 -1 且 errno==EINTR，下一轮 while 立刻检测标志
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, 500);

        // 🟢 8.0：EINTR 是 Ctrl+C/kill 正常打断，不算错误，continue 让 while 立即重判标志
        if(nready < 0) {
            if(errno == EINTR) continue;  // 被信号打断：正常情况，不打日志
            continue;                     // 其他错误：忽略，继续下一轮
        }


        // 遍历所有就绪事件
        // 在 6.0 版本中，主线程只会收到 listen fd 的事件
        // 客户端连接的事件全部由 Worker 处理
        for(int i = 0; i < nready; i++)
        {
            int fd = events[i].data.fd;

            //只有listen fd会触发事件
            if(fd == sockfd)
            {
                // ====================
                // ET 模式必须循环 accept
                // 直到返回 EAGAIN 为止
                // ====================
                while(1){
                    sockaddr_in cli_addr;   // 客户端地址
                    socklen_t cli_len = sizeof(cli_addr);

                    // 接受一个新连接
                    int cfd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);

                    if(cfd< 0)
                    {
                        if(errno == EAGAIN){
                            break;  //没有更多连接了，正常退出
                        }
                        //其他错误，继续尝试（不break）
                        continue;
                    }

                    // 新连接成功
                    LOG_DEBUG("accept 新连接: fd=%d", cfd);

                    // ====================
                    // 关键步骤：分发给 Worker
                    // ====================
                    // 1. 主线程设置非阻塞
                    setnonblocking(cfd);

                    // 2. 通过线程池分发给某个 Worker
                    // Worker 会自己设置 epoll 监听
                    g_thread_pool->distributeConnection(cfd);
                }
            }
        }
    }

    // ====================
    // 🟢 8.0：优雅关闭流程（按 Ctrl+C / kill -15 后会走到这里）
    // 顺序非常重要！必须严格按下面顺序来：
    //   Step 1：关闭监听 sockfd → 端口释放，不再接受新连接
    //   Step 2：停线程池 → Worker 的 running_=false，Worker 逐个 close 自己手里的连接 + join 等待线程退出
    //   Step 3：delete 线程池 → 释放 Worker 对象内存
    //   Step 4：关闭主线程的 epoll
    // ====================
    LOG_WARN("========================================");
    LOG_WARN("【优雅关闭】收到退出信号，开始关停流程");
    LOG_WARN("Step 1/4: 关闭监听 socket，不再接受新连接");
    LOG_WARN("========================================");

    // Step 1：关闭监听 fd（端口立刻释放，新连接连不上了）
    //        注意：必须放在最前面，防止关停过程中还在 accept 新连接
    close(sockfd);

    LOG_WARN("Step 2/4: 停止线程池（Worker 正在清理剩余连接，请稍候...）");
    // Step 2：停止所有 Worker
    //        Worker::stop() 会先 running_=false，然后 thread_.join() 等线程真正结束
    //        Worker 线程的 loop() 退出前会遍历 connections_ 逐个 close fd（在 worker.cpp:1151）
    g_thread_pool->stop();

    LOG_WARN("Step 3/4: 释放线程池内存");
    // Step 3：delete ThreadPool 对象（内部会 delete 每个 Worker）
    delete g_thread_pool;
    g_thread_pool = nullptr;

    LOG_WARN("Step 4/4: 关闭主线程 epoll");
    // Step 4：关闭主线程的 epoll 句柄
    close(epoll_fd);

    LOG_WARN("========================================");
    LOG_WARN("【优雅关闭】服务器已安全退出 ✓");
    LOG_WARN("========================================");
}