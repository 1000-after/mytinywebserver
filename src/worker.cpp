// =========================================
// 6.0 版本：Worker 工作线程实现
// 每个 Worker 有独立的 epoll，负责处理分配给它的所有连接
// =========================================
#include <stdio.h> // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset, memcmp
#include <unistd.h> // close, read, write
#include <fcntl.h>          // fcntl 设置非阻塞
#include <errno.h>      // 错误码
#include <sys/epoll.h>      // epoll 函数
#include <sys/eventfd.h>    // eventfd（线程间通知）
#include <arpa/inet.h>      // ntohl: 网络字节序转主机字节序

#include "worker.h" // Worker 类声明
#include "server.h"     // 通用工具函数和协议常量

// ==================== 构造函数 ====================
Worker::Worker():epoll_fd_(-1), // 初始化 epoll 为 -1（表示无效）
    notify_fd_(-1),// 初始化通知 fd 为 -1
    running_(false)// 初始状态为未运行
{

}


// ==================== 析构函数 ====================
Worker::~Worker()
{
    stop();     // 确保 Worker 已停止，避免资源泄漏
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
}

// ==================== 主循环（工作线程执行）====================
void Worker::loop()
{
    // 用于接收 epoll 事件的数组
    epoll_event events[MAX_EVENTS];

    // 初始化超时检查时间
    time_t last_check = time(nullptr);

    // 循环等待事件，直到 running_ 变为 false
    while(running_)
    {
        // 等待事件（超时 100ms）
        // 参数：epoll_fd, 事件数组, 数组大小, 超时时间(ms)
        int nready = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);

        if(nready <= 0)
        {
            // 没有事件，继续循环
            // 这里 continue 会让循环检查 running_ 状态
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
            // ====================
            {
                std::lock_guard<std::mutex> lock(mutex_);  // 加锁

                auto it = connections_.find(fd);
                if(it == connections_.end())
                {
                    continue;   // 找不到连接，跳过
                }

                // 标记是否需要关闭 fd
                bool need_close = false;

                // 处理可写事件（内核发送缓冲区有空了）
                if(ev & EPOLLOUT)
                {
                    handleWrite(it->second);
                }

                // 处理可读事件（客户端发来数据了）
                if(ev & EPOLLIN)
                {
                    handleRead(it->second);
                }

                // 如果连接已被关闭（handleRead/Write 可能会删除它）
                if(connections_.find(fd) == connections_.end())
                {
                    // 从 epoll 中移除
                    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                    need_close = true;   // 标记需要关闭
                }

                // 锁在这里自动释放

                // 锁外关闭 fd（避免死锁）
                if(need_close)
                {
                    close(fd);
                    printf("Worker 连接关闭: fd=%d\n", fd);
                }
            }
        }

        // 定期检查超时连接
        time_t now = time(nullptr);
        if(now - last_check >= CHECK_INTERVAL)
        {
            last_check = now;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                checkTimeout(); // 检查是否有连接超时
            }
        }
    }

    // Worker 退出前，清理所有连接
    // 遍历连接表，逐一关闭
    for(auto& pair : connections_)
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, pair.first, nullptr);
        close(pair.first);
    }
    connections_.clear();       // 清空连接表
    printf("Worker 主循环退出\n");
}

// ==================== 添加新连接 ====================
// 注意：此函数在主线程调用，需要加锁保护
void Worker::addConnection(int fd)
{
    // 用作用域限制锁的生命周期
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁

        // 1. 创建连接对象
        Connection conn;
        conn.fd = fd;
        conn.last_active_time = time(nullptr);

        // 2. 加入连接表
        connections_[fd] = conn;

        // 3. 设置非阻塞（ET 模式必须用非阻塞 socket）
        setnonblocking(fd);

        // 4. 加入 epoll 监听
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;  // 监听可读事件 + ET 边缘触发
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

        // 锁在这里自动释放（作用域结束）
    }

    // 5. 锁已释放，在锁外发送通知
    uint64_t notify_val = 1;
    write(notify_fd_, &notify_val, sizeof(notify_val));

    printf("Worker 收到新连接: fd=%d\n", fd);
}


// ==================== 处理读事件 ====================
void Worker::handleRead(Connection& conn)
{
    char tmp[BUF_SIZE];     // 临时读缓冲区

    // ET 模式必须循环读，直到返回 EAGAIN
    // 因为 ET 模式只在状态变化时触发一次
    while(1)
    {
        ssize_t n = read(conn.fd, tmp, BUF_SIZE);
        if( n > 0)
        {
            // 读到数据，追加到读缓冲区
            // insert 在 vector 末尾插入数据
            conn.read_buf.insert(
                conn.read_buf.end(),        // 插入位置：末尾
                tmp,        // 源数据开始位置
                tmp + n      // 源数据结束位置
            );

            // 刷新活跃时间（收到数据 = 连接活着）
            conn.last_active_time = time(nullptr);
        }
        else if(n == 0){
            // read 返回 0 表示客户端主动关闭连接
            // 从连接表中删除
            connections_.erase(conn.fd);
            printf("客户端主动断开: fd=%d\n", conn.fd);
            return;
        }
        else{
            // n < 0 出错
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 正常情况：内核缓冲区已读空
                break;      // 退出循环
            }
            // 真正的错误（如网络中断）
            connections_.erase(conn.fd);
            printf("读错误: fd=%d, errno=%d\n", conn.fd, errno);
            return;
        }
    }
    // 读完数据后，循环拆包（处理粘包）
    // 粘包：一次 read 可能收到多个完整包
    // 拆包：每次从 read_buf 中取出一个完整包处理
    while(1)
    {
        // 步骤1：检查是否够读包头（4字节）
        if(conn.read_buf.size() < sizeof(PacketHeader))
        {
            break;  // 包头都不够，等下次有更多数据
        }

        // 步骤2：解析包头
        // 将缓冲区前4个字节强转成 PacketHeader 结构体
        PacketHeader* header = (PacketHeader*)conn.read_buf.data();
        // 网络字节序转主机字节序（必须转换）
        uint32_t data_len = ntohl(header->data_len);

        // 步骤3：非法包检查
        if(data_len == 0 || data_len > MAX_PACKET_SIZE){
            printf("[错误] 非法包长度: %u\n", data_len);
            connections_.erase(conn.fd);
            return;
        }

        // 步骤4：计算完整包总长度
        // 总长度 = 包头4字节 + 数据体长度
        uint32_t total_len = sizeof(PacketHeader) + data_len;

        // 步骤5：检查是否够一个完整包
        if(conn.read_buf.size() < total_len)
        {
            break;  // 半包，等下次有更多数据
        }

        // 步骤6：获取数据部分指针
        // 跳过4字节包头，指向真正的业务数据
        char* data_ptr = conn.read_buf.data() + sizeof(PacketHeader);


        // 步骤7：判断包类型
        if(data_len == 9 && memcmp(data_ptr, "heartbeat", 9) == 0)
        {
            // ====================
            // 心跳包：只刷新活跃时间，不回显
            // ====================
            printf("fd=%d 收到心跳包\n", conn.fd);
            conn.last_active_time = time(nullptr);  // 刷新活跃时间
        }else{
            // ====================
            // 普通消息：将数据加入发送队列（异步发送）
            // ====================
            conn.write_buf.insert(
                conn.write_buf.end(),   // 插入位置：写缓冲区末尾
                data_ptr,   // 源数据开始位置
                data_ptr + data_len // 源数据结束位置
            );

            // 尝试立即发送（如果内核发送缓冲区有空）
            handleWrite(conn);
        }

        // 步骤8：从读缓冲区删除已处理的包
        conn.read_buf.erase(conn.read_buf.begin(), conn.read_buf.begin() + total_len);
    
    }
}

// ==================== 处理写事件 ====================
void Worker::handleWrite(Connection& conn)
{
    // 如果写缓冲区为空，直接返回
    // 没有数据要发送，就不需要处理写事件
    if(conn.write_buf.empty())
    {
        return;
    }

    // 循环发送，直到发完或遇到 EAGAIN
    while(!conn.write_buf.empty())
    {
        // 调用 write 将用户态数据复制到内核发送缓冲区
        ssize_t n = write(
            conn.fd,    // 目标文件描述符
            conn.write_buf.data(),  // 发送数据首地址
            conn.write_buf.size()   // 发送数据首地址
        );

        if(n > 0)
        {
            // 发送成功：从队列头部删除已发送的字节
            // FIFO 先进先出
            conn.write_buf.erase(
                conn.write_buf.begin(),     // 开始位置
                conn.write_buf.begin() + n      // 删除 n 个字节
            );
            // 刷新活跃时间（发送数据 = 连接活着）
            conn.last_active_time = time(nullptr);
        }else{
            // 发送失败
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 内核发送缓冲区满了
                // 开启 EPOLLOUT 监听，等内核有空再通知
                epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLOUT;   // 增加 EPOLLOUT
                ev.data.fd = conn.fd;
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);

                // 暂时发不了，但不是错误
                return;
            }
            // 真正错误（如客户端断开）
            connections_.erase(conn.fd);
            printf("写错误: fd=%d, errno=%d\n", conn.fd, errno);
            return;
        }
    }

    // 走到这里：写缓冲区已全部发完
    // 关闭 EPOLLOUT 监听（避免一直触发）
    // 因为只要内核可写，EPOLLOUT 事件会一直触发，浪费 CPU
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 移除 EPOLLOUT，只保留 EPOLLIN
    ev.data.fd = conn.fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev);
}


// ==================== 检查超时连接 ====================
void Worker::checkTimeout()
{
    time_t now = time(nullptr);     // 获取当前时间


    // 遍历所有连接
    // 使用迭代器方便删除元素
    auto it = connections_.begin();
    while(it != connections_.end())
    {
        // 判断：当前时间 - 最后活跃时间 > 超时时间
        if(now - it->second.last_active_time > IDLE_TIMEOUT){
            printf("连接超时踢出: fd=%d\n", it->first);
            // 从 epoll 中移除监听
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->first, nullptr);
            //关闭文件描述符
            close(it->first);
            // 从连接表删除（erase 返回下一个有效迭代器）
            it = connections_.erase(it);
        }else{
            // 未超时，继续检查下一个
            ++it;
        }
    }
}
