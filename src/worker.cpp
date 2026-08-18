// // =========================================
// // 6.0 版本：Worker 工作线程实现
// // 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
// // =========================================
// #include <stdio.h> // printf, perror
// #include <stdlib.h>     // exit
// #include <string.h>     // memset, memcmp
// #include <unistd.h> // close, read, write
// #include <fcntl.h>          // fcntl 设置非阻塞
// #include <errno.h>      // 错误码
// #include <sys/epoll.h>      // epoll 函数
// #include <sys/eventfd.h>    // eventfd（线程间通知）
// #include <arpa/inet.h>      // ntohl: 网络字节序转主机字节序

// #include "worker.h" // Worker 类声明
// #include "server.h"     // 通用工具函数和协议常量

// // ==================== 构造函数 ====================
// Worker::Worker():epoll_fd_(-1), // 初始化 epoll 为 -1（表示无效）
//     notify_fd_(-1),// 初始化通知 fd 为 -1
//     running_(false)// 初始状态为未运行
// {

// }


// // ==================== 析构函数 ====================
// Worker::~Worker()
// {
//     stop();     // 确保 Worker 已停止，避免资源泄漏
// }

// // ==================== 启动 Worker ====================
// void Worker::start()
// {
//     // 1. 创建 epoll 实例
//     // epoll_create1(0) 创建一个 epoll 句柄
//     epoll_fd_ = epoll_create1(0);
//     if(epoll_fd_ < 0)
//     {
//         perror("Worker epoll_create1 失败");
//         exit(EXIT_FAILURE);
//     }

//     // 2. 创建 eventfd（用于接收主线程通知）
//     // eventfd 是 Linux 提供的特殊 fd，专门用于线程间通知
//     // EFD_NONBLOCK 表示非阻塞模式
//     notify_fd_ = eventfd(0, EFD_NONBLOCK);
//     if(notify_fd_ < 0)
//     {
//         perror("Worker eventfd 失败");
//         exit(EXIT_FAILURE);
//     }

//     // 3. 将 notify_fd 加入 epoll 监听
//     // 当主线程通知时，这个 fd 会触发可读事件
//     epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发模式
//     ev.data.fd = notify_fd_;        // 存储 fd 标识
//     epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, notify_fd_, &ev);

//     // 4. 标记运行状态
//     running_ = true;

//     // 5. 创建并启动工作线程
//     // 使用 lambda 表达式捕获 this 指针，调用主循环
//     thread_ = std::thread([this](){
//             this->loop(); // 调用 Worker 的主循环函数
//         });

//     LOG_INFO("Worker[%p] 已启动，epoll_fd=%d", this, epoll_fd_);
// }   


// // ==================== 停止 Worker ====================
// void Worker::stop(){
//     if(!running_) return;       // 已经停止则直接返回
//     running_ = false;   // 标记为停止，循环会自动退出

//     // 如果线程还在运行，等待它结束
//     if(thread_.joinable())
//     {
//         thread_.join();     // 阻塞等待线程结束
//     }

//     // 关闭 epoll
//     if(epoll_fd_ >= 0)
//     {
//         close(epoll_fd_);
//         epoll_fd_ = -1;
//     }

//     // 关闭通知 fd
//     if(notify_fd_ >=0)
//     {
//         close(notify_fd_);
//         notify_fd_ = -1;
//     }

//     LOG_INFO("Worker[%p] 已停止", this);
// }

// // ==================== 主循环（工作线程执行）====================
// void Worker::loop()
// {
//     // 用于接收 epoll 事件的数组
//     epoll_event events[MAX_EVENTS];

//     // 初始化超时检查时间
//     time_t last_check = time(nullptr);

//     // 循环等待事件，直到 running_ 变为 false
//     while(running_)
//     {
//         // 等待事件（超时 100ms）
//         // 参数：epoll_fd, 事件数组, 数组大小, 超时时间(ms)
//         int nready = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);

//         if(nready <= 0)
//         {
//             // 没有事件，继续循环
//             // 这里 continue 会让循环检查 running_ 状态
//             continue;
//         }

//         // 遍历所有就绪事件
//         for(int i = 0; i < nready; ++i)
//         {
//             int fd = events[i].data.fd;     // 触发事件的 fd
//             int ev = events[i].events;  // 事件类型（读/写）

//             // ====================
//             // 情况1：通知 fd 有数据
//             // 表示主线程分配了新连接过来
//             // ====================
//             if(fd == notify_fd_)
//             {
//                 // 读取 eventfd 的计数（必须读，否则会一直触发）
//                 uint64_t notify_val;
//                 read(notify_fd_, &notify_val, sizeof(notify_val));

//                 // 新连接已通过 addConnection 加入，这里不需要额外处理
//                 // 只是清空 eventfd 的通知状态
//                 continue;
//             }

//             // ====================
//             // 情况2：客户端 fd 有事件
//             // ====================
//             {
//                 std::lock_guard<std::mutex> lock(mutex_);  // 加锁

//                 auto it = connections_.find(fd);
//                 if(it == connections_.end())
//                 {
//                     continue;   // 找不到连接，跳过
//                 }

//                 // 标记是否需要关闭 fd
//                 bool need_close = false;

//                 // 处理可写事件（内核发送缓冲区有空了）
//                 if(ev & EPOLLOUT)
//                 {
//                     handleWrite(it->second);
//                 }

//                 // 处理可读事件（客户端发来数据了）
//                 if(ev & EPOLLIN)
//                 {
//                     handleRead(it->second);
//                 }

//                 // 如果连接已被关闭（handleRead/Write 可能会删除它）
//                 if(connections_.find(fd) == connections_.end())
//                 {
//                     // 从 epoll 中移除
//                     epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
//                     need_close = true;   // 标记需要关闭
//                 }

//                 // 锁在这里自动释放

//                 // 锁外关闭 fd（避免死锁）
//                 if(need_close)
//                 {
//                     close(fd);
//                     printf("Worker 连接关闭: fd=%d\n", fd);
//                 }
//             }
//         }

//         // 定期检查超时连接
//         time_t now = time(nullptr);
//         if(now - last_check >= CHECK_INTERVAL)
//         {
//             last_check = now;
//             {
//                 std::lock_guard<std::mutex> lock(mutex_);
//                 checkTimeout(); // 检查是否有连接超时
//             }
//         }
//     }

//     // Worker 退出前，清理所有连接
//     // 遍历连接表，逐一关闭
//     for(auto& pair : connections_)
//     {
//         epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, pair.first, nullptr);
//         close(pair.first);
//     }
//     connections_.clear();       // 清空连接表
//     LOG_INFO("Worker[%p] 主循环退出", this);
// }

// // ==================== 添加新连接 ====================
// // 注意：此函数在主线程调用，需要加锁保护
// void Worker::addConnection(int fd)
// {
//     // 用作用域限制锁的生命周期
//     {
//         std::lock_guard<std::mutex> lock(mutex_);  // 加锁

//         // 1. 创建连接对象
//         Connection conn;
//         conn.fd = fd;
//         conn.last_active_time = time(nullptr);

//         // 2. 加入连接表
//         connections_[fd] = conn;

//         // 3. 设置非阻塞（ET 模式必须用非阻塞 socket）
//         setnonblocking(fd);

//         // 4. 加入 epoll 监听
//         epoll_event ev;
//         ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发
//         ev.data.fd = fd;
//         epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

//         // 锁在这里自动释放（作用域结束）
//     }

//     // 5. 锁已释放，在锁外发送通知
//     uint64_t notify_val = 1;
//     write(notify_fd_, &notify_val, sizeof(notify_val));

//     LOG_DEBUG("Worker 收到新连接: fd=%d", fd);
// }


// // ==================== 处理读事件 ====================
// void Worker::handleRead(Connection& conn)
// {
//     char tmp[BUF_SIZE];     // 临时读缓冲区

//     // ET 模式必须循环读，直到返回 EAGAIN
//     // 因为 ET 模式只在状态变化时触发一次
//     while(1)
//     {
//         ssize_t n = read(conn.fd, tmp, BUF_SIZE);
//         if( n > 0)
//         {
//             // 读到数据，追加到读缓冲区
//             // insert 在 vector 末尾插入数据
//             conn.read_buf.insert(
//                 conn.read_buf.end(),        // 插入位置：末尾
//                 tmp,        // 源数据开始位置
//                 tmp + n      // 源数据结束位置
//             );

//             // 刷新活跃时间（收到数据 = 连接活着）
//             conn.last_active_time = time(nullptr);
//         }
//         else if(n == 0){
//             // read 返回 0 表示客户端主动关闭连接
//             // 从连接表中删除
//             connections_.erase(conn.fd);
//             printf("客户端主动断开: fd=%d\n", conn.fd);
//             return;
//         }
//         else{
//             // n < 0 出错
//             if(errno == EAGAIN || errno == EWOULDBLOCK)
//             {
//                 // 正常情况：内核缓冲区已读空
//                 break;      // 退出循环
//             }
//             // 真正的错误（如网络中断）
//             connections_.erase(conn.fd);
//             LOG_ERROR("读错误: fd=%d, errno=%d", conn.fd, errno);
//             return;
//         }
//     }
//     // 读完数据后，循环拆包（处理粘包）
//     // 粘包：一次 read 可能收到多个完整包
//     // 拆包：每次从 read_buf 中取出一个完整包处理
//     while(1)
//     {
//         // 步骤1：检查是否够读包头（4字节）
//         if(conn.read_buf.size() < sizeof(PacketHeader))
//         {
//             break;  // 包头都不够，等下次有更多数据
//         }

//         // 步骤2：解析包头
//         // 将缓冲区前4个字节强转成 PacketHeader 结构体
//         PacketHeader* header = (PacketHeader*)conn.read_buf.data();
//         // 网络字节序转主机字节序（必须转换）
//         uint32_t data_len = ntohl(header->data_len);

//         // 步骤3：非法包检查
//         if(data_len == 0 || data_len > MAX_PACKET_SIZE){
//             printf("[错误] 非法包长度: %u\n", data_len);
//             connections_.erase(conn.fd);
//             return;
//         }

//         // 步骤4：计算完整包总长度
//         // 总长度 = 包头4字节 + 数据体长度
//         uint32_t total_len = sizeof(PacketHeader) + data_len;

//         // 步骤5：检查是否够一个完整包
//         if(conn.read_buf.size() < total_len)
//         {
//             break;  // 半包，等下次有更多数据
//         }

//         // 步骤6：获取数据部分指针
//         // 跳过4字节包头，指向真正的业务数据
//         char* data_ptr = conn.read_buf.data() + sizeof(PacketHeader);


//         // 步骤7：判断包类型
//         if(data_len == 9 && memcmp(data_ptr, "heartbeat", 9) == 0)
//         {
//             // ====================
//             // 心跳包：只刷新活跃时间，不回显
//             // ====================
//             printf("fd=%d 收到心跳包\n", conn.fd);
//             conn.last_active_time = time(nullptr);  // 刷新活跃时间
//         }else{
//             // ====================
//             // 普通消息：将数据加入发送队列（异步发送）
//             // ====================
//             conn.write_buf.insert(
//                 conn.write_buf.end(),   // 插入位置：写缓冲区末尾
//                 data_ptr,   // 源数据开始位置
//                 data_ptr + data_len // 源数据结束位置
//             );

//             // 尝试立即发送（如果内核发送缓冲区有空）
//             handleWrite(conn);
//         }

//         // 步骤8：从读缓冲区删除已处理的包
//         conn.read_buf.erase(conn.read_buf.begin(), conn.read_buf.begin() + total_len);
    
//     }
// }

// // ==================== 处理写事件 ====================
// void Worker::handleWrite(Connection& conn)
// {
//     // 如果写缓冲区为空，直接返回
//     // 没有数据要发送，就不需要处理写事件
//     if(conn.write_buf.empty())
//     {
//         return;
//     }

//     // 循环发送，直到发完或遇到 EAGAIN
//     while(!conn.write_buf.empty())
//     {
//         // 调用 write 将用户态数据复制到内核发送缓冲区
//         ssize_t n = write(
//             conn.fd,    // 目标文件描述符
//             conn.write_buf.data(),  // 发送数据首地址
//             conn.write_buf.size()   // 发送数据首地址
//         );

//         if(n > 0)
//         {
//             // 发送成功：从队列头部删除已发送的字节
//             // FIFO 先进先出
//             conn.write_buf.erase(
//                 conn.write_buf.begin(),     // 开始位置
//                 conn.write_buf.begin() + n      // 删除 n 个字节
//             );
//             // 刷新活跃时间（发送数据 = 连接活着）
//             conn.last_active_time = time(nullptr);
//         }else{
//             // 发送失败
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 // 内核发送缓冲区满了
//                 // 开启 EPOLLOUT 监听，等内核有空再通知
//                 epoll_event ev;
//                 ev.events = EPOLLIN | EPOLLET | EPOLLOUT;   // 增加 EPOLLOUT
//                 ev.data.fd = conn.fd;
//                 epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);

//                 // 暂时发不了，但不是错误
//                 return;
//             }
//             // 真正错误（如客户端断开）
//             connections_.erase(conn.fd);
//             LOG_ERROR("写错误: fd=%d, errno=%d", conn.fd, errno);
//             return;
//         }
//     }

//     // 走到这里：写缓冲区已全部发完
//     // 关闭 EPOLLOUT 监听（避免一直触发）
//     // 因为只要内核可写，EPOLLOUT 事件会一直触发，浪费 CPU
//     epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;  // 移除 EPOLLOUT，只保留 EPOLLIN
//     ev.data.fd = conn.fd;
//     epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);
// }


// // ==================== 检查超时连接 ====================
// void Worker::checkTimeout()
// {
//     time_t now = time(nullptr);     // 获取当前时间


//     // 遍历所有连接
//     // 使用迭代器方便删除元素
//     auto it = connections_.begin();
//     while(it != connections_.end())
//     {
//         // 判断：当前时间 - 最后活跃时间 > 超时时间
//         if(now - it->second.last_active_time > IDLE_TIMEOUT){
//             LOG_DEBUG("连接超时踢出: fd=%d", it->first);
//             // 从 epoll 中移除监听
//             epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->first, nullptr);
//             //关闭文件描述符
//             close(it->first);
//             // 从连接表删除（erase 返回下一个有效迭代器）
//             it = connections_.erase(it);
//         }else{
//             // 未超时，继续检查下一个
//             ++it;
//         }
//     }
// }






// // =========================================
// // 6.1 版本：Worker 工作线程实现
// // 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
// //协议头换成http协议
// // =========================================
// #include <stdio.h> // printf, perror
// #include <stdlib.h>     // exit
// #include <string.h>     // memset, memcmp
// #include <string>       // std::string
// #include <unistd.h> // close, read, write
// #include <fcntl.h>          // fcntl 设置非阻塞
// #include <errno.h>      // 错误码
// #include <sys/epoll.h>      // epoll 函数
// #include <sys/eventfd.h>    // eventfd（线程间通知）
// #include <arpa/inet.h>      // ntohl: 网络字节序转主机字节序

// #include "worker.h" // Worker 类声明
// #include "server.h"     // 通用工具函数和协议常量

// // ==================== 构造函数 ====================
// Worker::Worker():epoll_fd_(-1), // 初始化 epoll 为 -1（表示无效）
//     notify_fd_(-1),// 初始化通知 fd 为 -1
//     running_(false)// 初始状态为未运行
// {

// }


// // ==================== 析构函数 ====================
// Worker::~Worker()
// {
//     stop();     // 确保 Worker 已停止，避免资源泄漏
// }

// // ==================== 启动 Worker ====================
// void Worker::start()
// {
//     // 1. 创建 epoll 实例
//     // epoll_create1(0) 创建一个 epoll 句柄
//     epoll_fd_ = epoll_create1(0);
//     if(epoll_fd_ < 0)
//     {
//         perror("Worker epoll_create1 失败");
//         exit(EXIT_FAILURE);
//     }

//     // 2. 创建 eventfd（用于接收主线程通知）
//     // eventfd 是 Linux 提供的特殊 fd，专门用于线程间通知
//     // EFD_NONBLOCK 表示非阻塞模式
//     notify_fd_ = eventfd(0, EFD_NONBLOCK);
//     if(notify_fd_ < 0)
//     {
//         perror("Worker eventfd 失败");
//         exit(EXIT_FAILURE);
//     }

//     // 3. 将 notify_fd 加入 epoll 监听
//     // 当主线程通知时，这个 fd 会触发可读事件
//     epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发模式
//     ev.data.fd = notify_fd_;        // 存储 fd 标识
//     epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, notify_fd_, &ev);

//     // 4. 标记运行状态
//     running_ = true;

//     // 5. 创建并启动工作线程
//     // 使用 lambda 表达式捕获 this 指针，调用主循环
//     thread_ = std::thread([this](){
//             this->loop(); // 调用 Worker 的主循环函数
//         });

//     printf("Worker 已启动，epoll_fd=%d\n", epoll_fd_);
// }   


// // ==================== 停止 Worker ====================
// void Worker::stop(){
//     if(!running_) return;       // 已经停止则直接返回
//     running_ = false;   // 标记为停止，循环会自动退出

//     // 如果线程还在运行，等待它结束
//     if(thread_.joinable())
//     {
//         thread_.join();     // 阻塞等待线程结束
//     }

//     // 关闭 epoll
//     if(epoll_fd_ >= 0)
//     {
//         close(epoll_fd_);
//         epoll_fd_ = -1;
//     }

//     // 关闭通知 fd
//     if(notify_fd_ >=0)
//     {
//         close(notify_fd_);
//         notify_fd_ = -1;
//     }

//     printf("Worker 已停止\n");
// }

// // ==================== 主循环（工作线程执行）====================
// void Worker::loop()
// {
//     // 用于接收 epoll 事件的数组
//     epoll_event events[MAX_EVENTS];

//     // 初始化超时检查时间
//     time_t last_check = time(nullptr);

//     // 循环等待事件，直到 running_ 变为 false
//     while(running_)
//     {
//         // 等待事件（超时 100ms）
//         // 参数：epoll_fd, 事件数组, 数组大小, 超时时间(ms)
//         int nready = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);

//         if(nready <= 0)
//         {
//             // 没有事件，继续循环
//             // 这里 continue 会让循环检查 running_ 状态
//             continue;
//         }

//         // 遍历所有就绪事件
//         for(int i = 0; i < nready; ++i)
//         {
//             int fd = events[i].data.fd;     // 触发事件的 fd
//             int ev = events[i].events;  // 事件类型（读/写）

//             // ====================
//             // 情况1：通知 fd 有数据
//             // 表示主线程分配了新连接过来
//             // ====================
//             if(fd == notify_fd_)
//             {
//                 // 读取 eventfd 的计数（必须读，否则会一直触发）
//                 uint64_t notify_val;
//                 read(notify_fd_, &notify_val, sizeof(notify_val));

//                 // 新连接已通过 addConnection 加入，这里不需要额外处理
//                 // 只是清空 eventfd 的通知状态
//                 continue;
//             }

//             // ====================
//             // 情况2：客户端 fd 有事件
//             // ====================
//             {
//                 std::lock_guard<std::mutex> lock(mutex_);  // 加锁

//                 auto it = connections_.find(fd);
//                 if(it == connections_.end())
//                 {
//                     continue;   // 找不到连接，跳过
//                 }

//                 // 标记是否需要关闭 fd
//                 bool need_close = false;

//                 // 处理可写事件（内核发送缓冲区有空了）
//                 if(ev & EPOLLOUT)
//                 {
//                     handleWrite(it->second);
//                 }

//                 // 处理可读事件（客户端发来数据了）
//                 if(ev & EPOLLIN)
//                 {
//                     handleRead(it->second);
//                 }

//                 // 如果连接已被关闭（handleRead/Write 可能会删除它）
//                 if(connections_.find(fd) == connections_.end())
//                 {
//                     // 从 epoll 中移除
//                     epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
//                     need_close = true;   // 标记需要关闭
//                 }

//                 // 锁在这里自动释放

//                 // 锁外关闭 fd（避免死锁）
//                 if(need_close)
//                 {
//                     close(fd);
//                     printf("Worker 连接关闭: fd=%d\n", fd);
//                 }
//             }
//         }

//         // 定期检查超时连接
//         time_t now = time(nullptr);
//         if(now - last_check >= CHECK_INTERVAL)
//         {
//             last_check = now;
//             {
//                 std::lock_guard<std::mutex> lock(mutex_);
//                 checkTimeout(); // 检查是否有连接超时
//             }
//         }
//     }

//     // Worker 退出前，清理所有连接
//     // 遍历连接表，逐一关闭
//     for(auto& pair : connections_)
//     {
//         epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, pair.first, nullptr);
//         close(pair.first);
//     }
//     connections_.clear();       // 清空连接表
//     printf("Worker 主循环退出\n");
// }

// // ==================== 添加新连接 ====================
// // 注意：此函数在主线程调用，需要加锁保护
// void Worker::addConnection(int fd)
// {
//     // 用作用域限制锁的生命周期
//     {
//         std::lock_guard<std::mutex> lock(mutex_);  // 加锁

//         // 1. 创建连接对象
//         Connection conn;
//         conn.fd = fd;
//         conn.last_active_time = time(nullptr);

//         // 2. 加入连接表
//         connections_[fd] = conn;

//         // 3. 设置非阻塞（ET 模式必须用非阻塞 socket）
//         setnonblocking(fd);

//         // 4. 加入 epoll 监听
//         epoll_event ev;
//         ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发
//         ev.data.fd = fd;
//         epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

//         // 锁在这里自动释放（作用域结束）
//     }

//     // 5. 锁已释放，在锁外发送通知
//     uint64_t notify_val = 1;
//     write(notify_fd_, &notify_val, sizeof(notify_val));

//     printf("Worker 收到新连接: fd=%d\n", fd);
// }


// // ==================== 处理读事件（HTTP 协议）====================
// void Worker::handleRead(Connection& conn)
// {
//     char tmp[BUF_SIZE];

//     // 步骤1：循环读取数据（ET 模式必须读到 EAGAIN）
//     while(1)
//     {
//         ssize_t n = read(conn.fd, tmp, BUF_SIZE);
//         if(n > 0)
//         {
//             // 读到数据，追加到读缓冲区
//             conn.read_buf.insert(
//                 conn.read_buf.end(),
//                 tmp, tmp + n
//             );
//             conn.last_active_time = time(nullptr);
//         }
//         else if(n == 0)
//         {
//             // 客户端主动关闭连接
//             connections_.erase(conn.fd);
//             printf("客户端断开: fd=%d\n", conn.fd);
//             return;
//         }
//         else
//         {
//             if(errno == EAGAIN || errno == EWOULDBLOCK)
//             {
//                 break;  // 正常：内核缓冲区已读空
//             }
//             connections_.erase(conn.fd);
//             printf("读错误: fd=%d, errno=%d\n", conn.fd, errno);
//             return;
//         }
//     }

//     // 步骤2：解析 HTTP 请求
//     // 循环处理缓冲区中的数据（支持 keep-alive 多个请求）
//     while(!conn.http_parsed || !conn.read_buf.empty())
//     {
//         // 检查是否已解析过一个完整请求
//         if(conn.http_parsed)
//         {
//             // 已经解析并响应了一个请求
//             // 如果没有更多数据，等待下次读事件
//             if(conn.read_buf.empty()) break;
//             conn.http_parsed = false;  // 重置，解析下一个请求
//         }

//         // 检查 HTTP 头是否完整（需要找到 \r\n\r\n）
//         char* header_end = strstr(conn.read_buf.data(), "\r\n\r\n");
//         if(header_end == nullptr)
//         {
//             // HTTP 头不完整，等待更多数据
//             if(conn.read_buf.size() > MAX_HTTP_HEADER)
//             {
//                 // HTTP 头太大，可能是恶意请求
//                 printf("HTTP 头过大: fd=%d\n", conn.fd);
//                 connections_.erase(conn.fd);
//                 close(conn.fd);
//             }
//             break;
//         }

//         // 步骤3：解析请求行（第一行）
//         // 格式：GET /path HTTP/1.1\r\n
//         char* line_end = strstr(conn.read_buf.data(), "\r\n");
//         if(line_end == nullptr) break;

//         std::string request_line(conn.read_buf.data(), line_end - conn.read_buf.data());
//         printf("HTTP 请求: fd=%d, %s\n", conn.fd, request_line.c_str());

//         // 解析方法和路径
//         size_t space1 = request_line.find(' ');
//         if(space1 == std::string::npos)
//         {
//             // 请求行格式错误
//             conn.write_buf.insert(conn.write_buf.end(),
//                 HTTP_RESPONSE_404, HTTP_RESPONSE_404 + strlen(HTTP_RESPONSE_404));
//             handleWrite(conn);
//             connections_.erase(conn.fd);
//             close(conn.fd);
//             return;
//         }

//         conn.http_method = request_line.substr(0, space1);
//         std::string rest = request_line.substr(space1 + 1);
//         size_t space2 = rest.find(' ');
//         conn.http_path = (space2 != std::string::npos) ? rest.substr(0, space2) : "/";

//         // 步骤4：检查 Connection 头（keep-alive）
//         conn.http_keep_alive = true;  // 默认 keep-alive
//         const char* p = line_end + 2;
//         while(p < header_end)
//         {
//             const char* next = strstr(p, "\r\n");
//             if(next == nullptr || next > header_end) break;
//             std::string header(p, next - p);
//             if(header.find("Connection:") != std::string::npos &&
//                header.find("close") != std::string::npos)
//             {
//                 conn.http_keep_alive = false;
//             }
//             p = next + 2;
//         }

//         // 步骤5：删除已处理的请求数据
//         size_t header_len = (header_end - conn.read_buf.data()) + 4;
//         conn.read_buf.erase(conn.read_buf.begin(), conn.read_buf.begin() + header_len);

//         // 步骤6：生成 HTTP 响应
//         const char* response = HTTP_RESPONSE_OK;
//         size_t response_len = strlen(HTTP_RESPONSE_OK);

//         // 如果不是 keep-alive，在响应中设置 Connection: close
//         if(!conn.http_keep_alive)
//         {
//             response = HTTP_RESPONSE_404;  // 简单处理
//             response_len = strlen(HTTP_RESPONSE_404);
//         }

//         // 写入发送缓冲区
//         conn.write_buf.insert(conn.write_buf.end(), response, response + response_len);
//         conn.http_parsed = true;

//         // 步骤7：发送响应
//         handleWrite(conn);

//         // 如果不是 keep-alive，关闭连接
//         if(!conn.http_keep_alive && conn.write_buf.empty())
//         {
//             connections_.erase(conn.fd);
//             close(conn.fd);
//             printf("HTTP 请求完成(close): fd=%d\n", conn.fd);
//             return;
//         }
//     }
// }

// // ==================== 处理写事件 ====================
// void Worker::handleWrite(Connection& conn)
// {
//     // 如果写缓冲区为空，直接返回
//     // 没有数据要发送，就不需要处理写事件
//     if(conn.write_buf.empty())
//     {
//         return;
//     }

//     // 循环发送，直到发完或遇到 EAGAIN
//     while(!conn.write_buf.empty())
//     {
//         // 调用 write 将用户态数据复制到内核发送缓冲区
//         ssize_t n = write(
//             conn.fd,    // 目标文件描述符
//             conn.write_buf.data(),  // 发送数据首地址
//             conn.write_buf.size()   // 发送数据首地址
//         );

//         if(n > 0)
//         {
//             // 发送成功：从队列头部删除已发送的字节
//             // FIFO 先进先出
//             conn.write_buf.erase(
//                 conn.write_buf.begin(),     // 开始位置
//                 conn.write_buf.begin() + n      // 删除 n 个字节
//             );
//             // 刷新活跃时间（发送数据 = 连接活着）
//             conn.last_active_time = time(nullptr);
//         }else{
//             // 发送失败
//             if(errno == EAGAIN || errno == EWOULDBLOCK){
//                 // 内核发送缓冲区满了
//                 // 开启 EPOLLOUT 监听，等内核有空再通知
//                 epoll_event ev;
//                 ev.events = EPOLLIN | EPOLLET | EPOLLOUT;   // 增加 EPOLLOUT
//                 ev.data.fd = conn.fd;
//                 epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);

//                 // 暂时发不了，但不是错误
//                 return;
//             }
//             // 真正错误（如客户端断开）
//             connections_.erase(conn.fd);
//             printf("写错误: fd=%d, errno=%d\n", conn.fd, errno);
//             return;
//         }
//     }

//     // 走到这里：写缓冲区已全部发完
//     // 关闭 EPOLLOUT 监听（避免一直触发）
//     // 因为只要内核可写，EPOLLOUT 事件会一直触发，浪费 CPU
//     epoll_event ev;
//     ev.events = EPOLLIN | EPOLLET;  // 移除 EPOLLOUT，只保留 EPOLLIN
//     ev.data.fd = conn.fd;
//     epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);
// }


// // ==================== 检查超时连接 ====================
// void Worker::checkTimeout()
// {
//     time_t now = time(nullptr);     // 获取当前时间


//     // 遍历所有连接
//     // 使用迭代器方便删除元素
//     auto it = connections_.begin();
//     while(it != connections_.end())
//     {
//         // 判断：当前时间 - 最后活跃时间 > 超时时间
//         if(now - it->second.last_active_time > IDLE_TIMEOUT){
//             printf("连接超时踢出: fd=%d\n", it->first);
//             // 从 epoll 中移除监听
//             epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->first, nullptr);
//             //关闭文件描述符
//             close(it->first);
//             // 从连接表删除（erase 返回下一个有效迭代器）
//             it = connections_.erase(it);
//         }else{
//             // 未超时，继续检查下一个
//             ++it;
//         }
//     }
// }






// =========================================
// 7.0 版本：Worker 工作线程实现
// 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
//协议头换成http协议
//添加日志功能
// =========================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>

#include "worker.h"
#include "server.h"
#include "logger.h"      // 🆕 引入日志系统
#include "timer_wheel.h"   // 🆕 引入局部时间轮（每个 Worker 独立实例，无全局锁）
#include "config.h"        // 🆕 9.0：读取连接池配置参数

// ==================== 构造函数 ====================
Worker::Worker():epoll_fd_(-1), // 初始化 epoll 为 -1（表示无效）
    notify_fd_(-1),// 初始化通知 fd 为 -1
    running_(false),// 初始状态为未运行
    conn_pool_(nullptr)   // 🆕 9.0：连接池延迟到 start() 中初始化
{

}


// ==================== 析构函数 ====================
Worker::~Worker()
{
    stop();     // 确保 Worker 已停止，避免资源泄漏

    // 🆕 9.0：stop() 已把所有在用连接 release 回池，这里 delete 池子本身
    //        连接池析构会把空闲队列中的对象真正 delete 给 OS
    if (conn_pool_ != nullptr) {
        delete conn_pool_;
        conn_pool_ = nullptr;
    }
}

// ==================== 启动 Worker ====================
void Worker::start()
{
    // 1. 创建 epoll 实例
    // epoll_create1(0) 创建一个 epoll 句柄
    epoll_fd_ = epoll_create1(0);
    if(epoll_fd_ < 0)
    {
        perror("Worker epoll_create1 失败");
        exit(EXIT_FAILURE);
    }

    // 2. 创建 eventfd（用于接收主线程通知）
    // eventfd 是 Linux 提供的特殊 fd，专门用于线程间通知
    // EFD_NONBLOCK 表示非阻塞模式
    notify_fd_ = eventfd(0, EFD_NONBLOCK);
    if(notify_fd_ < 0)
    {
        perror("Worker eventfd 失败");
        exit(EXIT_FAILURE);
    }

    // 3. 将 notify_fd 加入 epoll 监听
    // 当主线程通知时，这个 fd 会触发可读事件
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发模式
    ev.data.fd = notify_fd_;        // 存储 fd 标识
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, notify_fd_, &ev);

    // 🆕 9.0：3.5 创建本 Worker 独立的连接池（分片）
    //   每个 Worker 各有各的池子，减少跨线程锁竞争
    //   预分配数量 = max_connections / Worker 数（均分），保底 64 个
    {
        int max_conns = Config::instance().getInt("server.max_connections", 1000);
        int workers   = Config::instance().getInt("performance.threads", 4);
        size_t init_cnt = (size_t)(max_conns / (workers > 0 ? workers : 1));
        if (init_cnt < 64)  init_cnt = 64;              // 保底 64 个
        size_t buf_sz   = (size_t)Config::instance().getInt("pool.reserve_bytes", 8192);
        size_t max_cnt  = (size_t)(max_conns * 2);      // 上限 = 最大连接数 ×2（留余量）
        conn_pool_ = new ConnectionPool(init_cnt, buf_sz, max_cnt);
    }
    LOG_INFO("Worker 连接池就绪，空闲=%zu，累计=%zu",
             conn_pool_->freeCount(), conn_pool_->totalCount());

    // 🆕 10.0：初始化局部时间轮（超时秒数从配置读，默认 15）
    int timeout = Config::instance().getInt("server.timeout", 15);
    timer_wheel_.init(timeout);

    // 4. 标记运行状态
    running_ = true;

    // 5. 创建并启动工作线程
    // 使用 lambda 表达式捕获 this 指针，调用主循环
    thread_ = std::thread([this](){
            this->loop(); // 调用 Worker 的主循环函数
        });

    printf("Worker 已启动，epoll_fd=%d\n", epoll_fd_);
}   


// ==================== 停止 Worker ====================
void Worker::stop(){
    if(!running_) return;       // 已经停止则直接返回
    running_ = false;   // 标记为停止，循环会自动退出

    // 如果线程还在运行，等待它结束
    if(thread_.joinable())
    {
        thread_.join();     // 阻塞等待线程结束
    }

    // 关闭 epoll
    if(epoll_fd_ >= 0)
    {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }

    // 关闭通知 fd
    if(notify_fd_ >=0)
    {
        close(notify_fd_);
        notify_fd_ = -1;
    }

    printf("Worker 已停止\n");

    // 🆕 9.0：stop 收尾——把所有仍在 connections_ 里的在用连接 release 回池
    //   注意：这里运行顺序是「running_=false → join 等线程退出 → 关 epoll/notify_fd → 执行本清理」
    //        所以此时已经没有并发访问，不需要加 mutex_
    if (conn_pool_ != nullptr) {
        for (auto it = connections_.begin(); it != connections_.end(); ) {
            Connection* c = it->second;
            // 从 epoll 移除（虽然 epoll_fd_ 已经 close 了，保险起见）
            // if (epoll_fd_ >= 0) epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->first, nullptr);
            // 关闭 socket fd
            close(it->first);
            // release 回池（close(fd) 后归还——池子不负责关 fd）
            conn_pool_->release(c);
            // 从哈希表移除（注意 release 没有析构 c，只是放回池子）
            it = connections_.erase(it);
        }
    }
}

// ==================== 主循环（工作线程执行）====================
void Worker::loop()
{
    // 用于接收 epoll 事件的数组
    epoll_event events[MAX_EVENTS];

    // 🆕 10.0：局部时间轮 tick 驱动
    time_t last_tick = time(nullptr);

    // 循环等待事件，直到 running_ 变为 false
    // 🟢 10.0 改造：超时管理由全局 TimerWheel 后台线程 → 局部 timer_wheel_ 在 loop() 中驱动
    while(running_)
    {
        // 等待事件（超时 100ms）
        int nready = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);

        // 🆕 10.0：不管有没有事件，都检查是否该推进时间轮
        time_t now = time(nullptr);
        if (now - last_tick >= 1) {
            // 在锁内推进时间轮，拿到过期 fd 列表
            std::vector<int> expired_fds;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                expired_fds = timer_wheel_.tick();
            }
            // 锁外处理过期连接（关闭 fd + 从 epoll 移除 + 从连接表删除）
            for(int fd : expired_fds) {
                // tryCloseTimeoutConnection 会在锁内操作 timer_wheel_/connections_
                tryCloseTimeoutConnection(fd);
            }
            last_tick = now;
        }

        if(nready <= 0)
        {
            continue;
        }

        // 遍历所有就绪事件
        for(int i = 0; i < nready; ++i)
        {
            int fd = events[i].data.fd;     // 触发事件的 fd
            int ev = events[i].events;  // 事件类型（读/写）

            // ====================
            // 情况1：通知 fd 有数据
            // 表示主线程分配了新连接过来
            // ====================
            if(fd == notify_fd_)
            {
                // 读取 eventfd 的计数（必须读，否则会一直触发）
                uint64_t notify_val;
                read(notify_fd_, &notify_val, sizeof(notify_val));

                // 新连接已通过 addConnection 加入，这里不需要额外处理
                // 只是清空 eventfd 的通知状态
                continue;
            }

            // ====================
            // 情况2：客户端 fd 有事件
            // 🟢 修复：所有 erase/close 动作统一交给 loop()
            //      handleRead/Write 只设置两个布尔标记位，绝不直接 close/erase
            // ====================
            bool need_close = false;
            bool close_after_unlock = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);  // 加锁

                auto it = connections_.find(fd);
                if(it == connections_.end()) continue;  // 找不到连接，跳过

                // 处理可写事件
                if(ev & EPOLLOUT)
                {
                    bool nc = false, cau = false;
                    // 🆕 9.0：it->second 现在是 Connection*，要解引用传引用
                    handleWrite(*(it->second), nc, cau);
                    if(nc)  need_close = true;
                    if(cau) close_after_unlock = true;
                }

                // 处理可读事件（先重新find，因为handleWrite可能触发erase标记位）
                if(ev & EPOLLIN)
                {
                    auto it2 = connections_.find(fd);
                    if(it2 != connections_.end())
                    {
                        bool nc = false, cau = false;
                        // 🆕 9.0：同样解引用
                        handleRead(*(it2->second), nc, cau);
                        if(nc)  need_close = true;
                        if(cau) close_after_unlock = true;
                    }
                }

                // 如果 handleRead/Write 设置了 need_close，这里执行唯一一次 erase + EPOLL_CTL_DEL
                if(need_close)
                {
                    auto it3 = connections_.find(fd);
                    if(it3 != connections_.end())
                    {
                        timer_wheel_.removeConnection(fd);   // 🆕 10.0：局部时间轮
                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);

                        // 🆕 9.0：先拿 Connection* 指针再 erase
                        Connection* c = it3->second;
                        connections_.erase(it3);
                        // release 回池（只清脏数据，不析构不释放底层内存）
                        if (conn_pool_ != nullptr) {
                            conn_pool_->release(c);
                        } else {
                            delete c;
                        }
                    }
                }
            }   // 🔓 锁在这里释放

            // 锁外只 close 一次（避免死锁/重复关）
            if(need_close && close_after_unlock)
            {
                close(fd);
            }
        }

        // 🆕 10.0：旧的全局 TimerWheel 回调跨 Worker 查找已移除
        //   现在由局部 timer_wheel_.tick() 在 loop() 中直接处理

    }

    // Worker 退出前，清理所有连接
    // 遍历连接表，逐一关闭
    for(auto& pair : connections_)
    {
        timer_wheel_.removeConnection(pair.first);   // 🆕 10.0：从局部时间轮移除
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, pair.first, nullptr);
        close(pair.first);
        // 🆕 9.0：对象从池子来的，关完 fd 后 release 回池
        if (conn_pool_ != nullptr) {
            conn_pool_->release(pair.second);
        } else {
            // 兜底：如果池子没初始化（极端情况），直接 delete 防泄漏
            delete pair.second;
        }
    }
    connections_.clear();       // 清空连接表（此时 value 都是已经 release 的悬空指针，但 map 自己不管对象）
    printf("Worker 主循环退出\n");
}

// ==================== 添加新连接 ====================
// 注意：此函数在主线程调用，需要加锁保护
void Worker::addConnection(int fd)
{
    // 用作用域限制锁的生命周期
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁

        // 🆕 9.0：1. 从连接池 acquire 一个预分配好的对象（不再在栈上创建+拷贝）
        //        从池里拿出来的对象：read_buf/write_buf 已经 reserve(8KB)，http_method/path 也已预留
        Connection* c = conn_pool_->acquire();
        c->fd = fd;
        c->last_active_time = time(nullptr);

        // 2. 加入连接表（存指针，不再存对象副本）
        connections_[fd] = c;

        // 3. 设置非阻塞（ET 模式必须用非阻塞 socket）
        setnonblocking(fd);

        // 4. 加入 epoll 监听
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

        // 🆕 10.0：新连接加入局部时间轮（超时秒数由配置决定）
        timer_wheel_.addConnection(fd);

        // 锁在这里自动释放（作用域结束）
    }

    // 5. 锁已释放，在锁外发送通知
    uint64_t notify_val = 1;
    write(notify_fd_, &notify_val, sizeof(notify_val));

    LOG_INFO("Worker 收到新连接: fd=%d, pool_free=%zu", fd,
             conn_pool_ ? conn_pool_->freeCount() : 0);
}

// ==================== 🆕 10.0：局部时间轮超时关闭 ====================
// 与旧 tryCloseConnection 的区别：
//   - 旧版：全局 TimerWheel 回调 → ThreadPool 轮询所有 Worker 查找
//   - 新版：局部 timer_wheel_.tick() 直接返回过期 fd，本 Worker 直接关
// 调用时已在锁外（tick 返回后再逐个调用本函数，本函数内部自己加锁）
void Worker::tryCloseTimeoutConnection(int fd)
{
    bool do_close_fd = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = connections_.find(fd);
        if(it == connections_.end()) return;  // 不属于本 Worker，直接返回

        // 1) 从时间轮移除（已过期，防重复操作）
        timer_wheel_.removeConnection(fd);

        // 2) 从 epoll 移除
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);

        // 3) 取出 Connection* 指针
        Connection* c = it->second;

        // 4) 从连接表移除
        connections_.erase(it);

        // 5) release 回池
        if (conn_pool_ != nullptr) {
            conn_pool_->release(c);
        } else {
            delete c;
        }

        do_close_fd = true;
        LOG_WARN("[TimerWheel-local] 连接超时，关闭: fd=%d", fd);
    }

    // 锁外真正关 fd
    if(do_close_fd) close(fd);
}


// ==================== 处理读事件（HTTP 协议）====================
void Worker::handleRead(Connection& conn, bool& need_close, bool& close_after_unlock)
{
    need_close = false;
    close_after_unlock = false;
    char tmp[BUF_SIZE];

    // 步骤1：循环读取数据（ET 模式必须读到 EAGAIN）
    while(1)
    {
        ssize_t n = read(conn.fd, tmp, BUF_SIZE);
        if(n > 0)
        {
            conn.read_buf.insert(conn.read_buf.end(), tmp, tmp + n);
            conn.last_active_time = time(nullptr);
            // 🆕 10.0：读到数据 → 推迟超时（局部时间轮，无锁竞争，直接刷）
            timer_wheel_.refreshConnection(conn.fd);
        }
        else if(n == 0)
        {
            // 🟢 客户端主动断开 → 只打标记，loop() 里统一 erase+close
            LOG_DEBUG("客户端主动断开: fd=%d", conn.fd);
            need_close = true; close_after_unlock = true;
            return;
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;  // 正常：内核缓冲区已读空
            }
            // 🟢 读错误 → 只打标记
            LOG_WARN("读错误: fd=%d, errno=%d", conn.fd, errno);
            need_close = true; close_after_unlock = true;
            return;
        }
    }

    // 步骤2：解析 HTTP 请求（支持 keep-alive 多个请求）
    while(!conn.http_parsed || !conn.read_buf.empty())
    {
        if(conn.http_parsed)
        {
            if(conn.read_buf.empty()) break;
            conn.http_parsed = false;
        }

        char* header_end = strstr(conn.read_buf.data(), "\r\n\r\n");
        if(header_end == nullptr)
        {
            if(conn.read_buf.size() > MAX_HTTP_HEADER)
            {
                // 🟢 头太大 → 打标记，不直接关
                LOG_WARN("HTTP 头过大: fd=%d", conn.fd);
                need_close = true; close_after_unlock = true;
            }
            break;
        }

        char* line_end = strstr(conn.read_buf.data(), "\r\n");
        if(line_end == nullptr) break;

        std::string request_line(conn.read_buf.data(), line_end - conn.read_buf.data());
        LOG_DEBUG("HTTP 请求: fd=%d, %s", conn.fd, request_line.c_str());

        size_t space1 = request_line.find(' ');
        if(space1 == std::string::npos)
        {
            // 🟢 格式错误：写 404，发，关
            conn.write_buf.insert(conn.write_buf.end(),
                HTTP_RESPONSE_404, HTTP_RESPONSE_404 + strlen(HTTP_RESPONSE_404));
            bool wnc = false, wcau = false;
            handleWrite(conn, wnc, wcau);
            need_close = true; close_after_unlock = true;
            return;
        }

        conn.http_method = request_line.substr(0, space1);
        std::string rest = request_line.substr(space1 + 1);
        size_t space2 = rest.find(' ');
        conn.http_path = (space2 != std::string::npos) ? rest.substr(0, space2) : "/";

        // 🆕 步骤3.5：解析 HTTP 版本号，决定「默认连接策略」（RFC 标准！非常关键）
        //   - HTTP/1.0 → 没写 Connection 头时默认 = close (短连接)
        //   - HTTP/1.1 → 没写 Connection 头时默认 = keep-alive (长连接)
        bool is_http11 = true;   // 默认假设 HTTP/1.1（curl/浏览器都是）
        if(space2 != std::string::npos)
        {
            std::string ver = rest.substr(space2 + 1);
            if(ver.find("HTTP/1.0") != std::string::npos) is_http11 = false;
            // HTTP/2 及以上也默认 keep-alive，保持 true
        }

        // 🆕 步骤4：解析 Connection 头（基于版本的默认值 + 显式头覆盖）
        //   先根据版本给默认值，再用 Connection 头里的显式指令覆盖
        //   🟢 重要：HTTP 头是大小写不敏感的（RFC 7230），ab 会发 "Keep-Alive" 大写，
        //      curl 会发 "keep-alive" 小写，必须统一转成小写再匹配，否则会误判！
        conn.http_keep_alive = is_http11;   // 1.1→true  1.0→false
        const char* p = line_end + 2;
        while(p < header_end)
        {
            const char* next = strstr(p, "\r\n");
            if(next == nullptr || next > header_end) break;
            std::string header(p, next - p);
            // 👉 把这一行头转成全小写，再做 find 查找（彻底消除大小写差异）
            std::string header_lower;
            header_lower.reserve(header.size());
            for(char c : header) header_lower.push_back((char)std::tolower((unsigned char)c));

            // 🟢 用小写版本查找 "connection:"、"close"、"keep-alive"
            if(header_lower.find("connection:") != std::string::npos)
            {
                if(header_lower.find("close") != std::string::npos)      conn.http_keep_alive = false;
                if(header_lower.find("keep-alive") != std::string::npos) conn.http_keep_alive = true;
            }
            p = next + 2;
        }

        size_t header_len = (size_t)(header_end - conn.read_buf.data()) + 4;
        conn.read_buf.erase(conn.read_buf.begin(), conn.read_buf.begin() + header_len);

        // 步骤6：生成 HTTP 响应
        // 🟢 修复：不要再把 Connection: close 请求一律回 404！
        //   - path == "/" 并且客户端要 keep-alive → 200 + keep-alive
        //   - path == "/" 并且客户端要 close     → 200 + close（新增常量）
        //   - 其他 path → 404 + close（找不到）
        const char* response;
        size_t response_len;
        if(conn.http_path == "/")
        {
            if(conn.http_keep_alive) {
                response = HTTP_RESPONSE_OK;
                response_len = strlen(HTTP_RESPONSE_OK);
            } else {
                response = HTTP_RESPONSE_OK_CLOSE;   // 🆕 200 OK + Connection: close
                response_len = strlen(HTTP_RESPONSE_OK_CLOSE);
            }
        }
        else
        {
            // 只在 path 非法时才回 404
            response = HTTP_RESPONSE_404;
            response_len = strlen(HTTP_RESPONSE_404);
            conn.http_keep_alive = false;  // 404 一律关闭连接
        }
        conn.write_buf.insert(conn.write_buf.end(), response, response + response_len);
        conn.http_parsed = true;

        // 🟢 调用 handleWrite 发响应（传标记位）
        bool wnc = false, wcau = false;
        handleWrite(conn, wnc, wcau);
        if(wnc)
        {
            need_close = true; close_after_unlock = true;
            return;
        }

        // Connection: close 且已写完 → 关连接
        if(!conn.http_keep_alive && conn.write_buf.empty())
        {
            LOG_DEBUG("HTTP 请求完成(close): fd=%d", conn.fd);
            need_close = true; close_after_unlock = true;
            return;
        }
    }
}

// ==================== 处理写事件 ====================
void Worker::handleWrite(Connection& conn, bool& need_close, bool& close_after_unlock)
{
    need_close = false;
    close_after_unlock = false;

    if(conn.write_buf.empty()) return;

    while(!conn.write_buf.empty())
    {
        ssize_t n = write(conn.fd, conn.write_buf.data(), conn.write_buf.size());
        if(n > 0)
        {
            conn.write_buf.erase(conn.write_buf.begin(), conn.write_buf.begin() + n);
            conn.last_active_time = time(nullptr);
            timer_wheel_.refreshConnection(conn.fd);   // 🆕 10.0：写出数据 → 推迟超时
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
                ev.data.fd = conn.fd;
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);
                return;   // 发不了，不是错误
            }
            // 🟢 真正错误：只打标记，loop() 负责唯一 erase+close
            LOG_WARN("写错误: fd=%d, errno=%d", conn.fd, errno);
            need_close = true; close_after_unlock = true;
            return;
        }
    }

    // 缓冲区已发完：关闭 EPOLLOUT 监听
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = conn.fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);
}
