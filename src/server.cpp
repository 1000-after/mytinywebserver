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
#include <stdio.h> // 输入输出：printf打印日志、perror打印系统错误
#include <stdlib.h> // 通用工具：内存分配、退出
#include <string.h> // 内存操作：memset、strlen
#include <unistd.h> // Unix系统调用：close、read、write

// Linux网络编程核心头文件
#include <sys/socket.h> // socket、bind、listen、accept、send、recv
#include <netinet/in.h> // sockaddr_in结构体、htons/ntohl字节序转换
#include <fcntl.h> // fcntl：设置非阻塞模式
#include <errno.h> // 错误码：EAGAIN、EWOULDBLOCK等
#include <sys/epoll.h> // epoll_create1、epoll_ctl、epoll_wait

// C++容器
#include <unordered_map> // 哈希表：快速查找客户端连接

// 自定义头文件（Connection结构体定义）
#include <server.h>

// ==============================================
// 宏定义（全局配置）
// ==============================================
#define MAX_EVENTS 1024 // epoll_wait一次最多返回多少个事件
#define BUF_SIZE 1024 // 每次从socket读取的临时缓冲区大小
#define MAX_PACKET_SIZE 65536 // 最大允许的数据包大小（64KB），防恶意攻击

// ==============================================
// 全局变量
// ==============================================
// 静态全局哈希表：存储所有客户端连接
// key：文件描述符fd
// value：Connection结构体（包含fd、读缓冲区）
static std::unordered_map<int,Connection> g_connections;


// ==============================================
// 函数：将fd设置为非阻塞模式
// 作用：ET模式必须搭配非阻塞socket，否则会卡死
// ==============================================
int setnonblocking(int fd){
     // 第一步：获取fd原来的flag（状态标记）
     int old_flag = fcntl(fd, F_GETFL);
     int new_flag = old_flag | O_NONBLOCK;
     fcntl(fd, F_SETFL, new_flag);
     return old_flag;
}

// ==============================================
// 函数：将fd添加到epoll监听
// 模式：ET边缘触发 + 只监听读事件EPOLLIN
// ==============================================
void epollAddFd(int epoll_fd, int fd){
    struct epoll_event ev;
    
    // ET模式 + 监听可读事件
    ev.events = EPOLLIN | EPOLLET;

    // 事件关联的数据：只需要存fd
    ev.data.fd = fd;

    // 添加到epoll监听列表
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    

    // 顺手设置非阻塞（ET必须非阻塞）
    setnonblocking(fd);
}


// ==============================================
// 【工业级协议头】
// 1. #pragma pack(1) 强制1字节对齐 → 不填充空白字节
// 2. uint32_t 固定4字节 → 跨平台不变
// 3. 网络传输必须用：包头(4字节) + 数据体
// ==============================================
#pragma pack(push,1)
struct PacketHeader{
    uint32_t data_len;  // 数据体长度（必须是4字节无符号整数）
};
#pragma pack(pop)
// ==============================================

// ==============================================
// 【核心拆包函数】
// 功能：从缓冲区里拆出一个完整的包
// 返回值：true=拆到一个完整包 false=数据不够/非法包
// ==============================================
static bool parseOnePacket(Connection& conn){
       // ----------------------
    // 步骤1：判断是否够读包头（4字节）
    // ----------------------
    if(conn.read_buf.size() < sizeof(PacketHeader)){
        return false;   //包头都不够->半包，等下次
    }

    // ----------------------
    // 步骤2：从缓冲区头部强转成包头指针
    // ----------------------
    PacketHeader* header = (PacketHeader*)conn.read_buf.data();

    // ----------------------
    // 步骤3：网络字节序 → 转主机字节序（必须转！否则数字是乱的）
    // ----------------------
    uint32_t data_len = ntohl(header->data_len);

     // ----------------------
    // 步骤4：安全检查：防止恶意超长包/空包
    // ----------------------
    if(data_len == 0 || data_len > MAX_PACKET_SIZE){
        printf("[错误] 非法包长度：%u\n", data_len);
        return false;
    }

    // ----------------------
    // 步骤5：计算一个完整包的总长度
    // 总长度 = 包头4字节 + 数据长度
    // ----------------------
    uint32_t total_len = sizeof(PacketHeader) + data_len;

    // ----------------------
    // 步骤6：判断缓冲区是否够一个完整包
    // ----------------------
    if(conn.read_buf.size() < total_len){
        return false;   //不够->半包
    }

    // ----------------------
    // ✅ 到这里：已经确定收到一个完整合法包
    // ----------------------

    // 数据指针 = 缓冲区起始 + 跳过包头4字节
    char* data_ptr = conn.read_buf.data() + sizeof(PacketHeader);

    // 回显给客户端：只发送真实数据部分
    write(conn.fd, data_ptr, data_len);

    //打印日志
    printf("[合法拆包] fd=%d 数据长度=%u\n", conn.fd, data_len);

    // ----------------------
    // 步骤7：把已经处理完的包从缓冲区删掉
    // ----------------------
    conn.read_buf.erase(
        conn.read_buf.begin(),  //从开头删
        conn.read_buf.begin() + total_len // 删到整个包结束
    );
    return true;    //成功拆一个包
}



// ==============================================
// 函数：ET模式循环读取数据 → 存入用户层缓冲区
// 作用：一次把内核缓冲区数据读空
// ==============================================
static bool readToBuffer(int fd){
    // 从全局哈希表找到当前连接
    auto it = g_connections.find(fd);
    if(it == g_connections.end()){
        return false; // 找不到，直接返回
    }

    // 临时数组：接收read读取的数据
    char tmp[BUF_SIZE];


    // ET模式必须循环读，直到返回EAGAIN
    while(1){
        // 从socket读取数据到tmp数组
        ssize_t n = read(fd, tmp, BUF_SIZE);

        if(n > 0){
            // 读到数据 → 追加到连接的读缓冲区
            it->second.read_buf.insert(
                it->second.read_buf.end(),
                tmp,
                tmp + n
            );
        }
        else if(n == 0){
            // read返回0 → 客户端关闭连接
            return false;
        }
        else{
            // n < 0 出错
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                 // 没有数据了 → 正常退出循环
                 break;
            }
            // 其他错误 → 关闭连接
            return false;
        }
    }

     // ----------------------
    // 数据读完 → 循环拆包（可能一次收到多个包：粘包）
    // ----------------------
    while (parseOnePacket(it->second));
    return true;
}

// ==============================================
// 函数：关闭并清理一个客户端连接
// ==============================================
static void closeConnection(int epoll_fd, int fd){
    // 1. 从epoll中移除监听
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    // 2. 关闭socket文件描述符
    close(fd);
    // 3. 从全局连接表删除
    g_connections.erase(fd);
}

// ==============================================
// 主函数：启动服务器
// ==============================================
void runServer(uint16_t ports){
    // ----------------------
    // 1. 创建TCP socket
    // ----------------------
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        perror("socket创建失败");
        return;
    }

    // ----------------------
    // 2. 设置端口复用（防止重启报错address in use）
    // ----------------------
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    // ----------------------
    // 3. 绑定IP和端口
    // ----------------------
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ports);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind绑定失败");
        close(sockfd);
        return;
    }

    // ----------------------
    // 4. 开始监听
    // ----------------------
    if(listen(sockfd, 100) < 0){
        perror("listen监听失败");
        close(sockfd);
        return;
    }

    // ----------------------
    // 5. 创建epoll实例
    // ----------------------
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0){
        perror("创建失败");
        close(sockfd);
        return;
    }

    // ----------------------
    // 6. 将监听socket加入epoll
    // ----------------------
    epollAddFd(epoll_fd, sockfd);


    // 存储epoll返回的事件
    struct epoll_event events[MAX_EVENTS];
    printf("【工业级拆包服务器】启动成功 端口:%d\n", ports);

    // ----------------------
    // 7. 主线程死循环：epoll事件驱动
    // ----------------------

    while(1){
        // 等待事件（-1=永久阻塞）
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(nready <= 0) continue;

        //便利所有就绪事件
        for(int i = 0; i < nready; i++){
            int fd = events[i].data.fd;

            if(fd == sockfd){
                // ----------------------
                // 事件1：有新客户端连接
                // ----------------------
                while(1){
                    sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);

                    //接收连接
                    int cfd = accept(sockfd, (sockaddr*)&cli_addr, &cli_len);
                    if(cfd < 0){
                        if(errno == EAGAIN) break;  //没有更多连接
                        continue;
                    }
                    // 新建连接对象
                    Connection conn;
                    conn.fd = cfd;

                    //加入全局表
                    g_connections[cfd] = conn;
                    //加入epoll监听
                    epollAddFd(epoll_fd, cfd);
                }
            }
            else{
                // ----------------------
                // 事件2：客户端发来数据 / 断开连接
                // ----------------------
                bool ok = readToBuffer(fd);
                if(!ok){
                    // 读取失败/断开 → 关闭连接
                    closeConnection(epoll_fd, fd);
                }
            }
        }
    }
    // ----------------------
    // 理论上永远不会走到这里（while死循环）
    // ----------------------
    close(epoll_fd);
    close(sockfd);
}