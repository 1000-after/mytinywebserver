#include <iostream>
#include <sys/socket.h>//创建套机字
#include <netinet/in.h> //网络字主机字转换
#include <unistd.h>
#include <string.h>
#include "server.h"
#include <fcntl.h>
#include <errno.h>
void runServer(uint16_t ports){
    //1.创建电话机创建套接字
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        std::cout <<"创建套接字失败" << std::endl;
        perror("socket");
        return;
    }

    //2.创建地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ports);

    //3.绑定电话机
    if(bind(sockfd, (struct sockaddr*)&addr,sizeof(addr)) < 0){
        std::cout <<"绑定电话机" << std::endl;
        perror("bind");
        close(sockfd);
        return;
    }

    //4.监听
    if (listen(sockfd, 5) == -1) {
            std::cout <<"监听失败" << std::endl;
            perror("listen");
            close(sockfd);
        return;
    }
    std::cout <<"开启监听（非阻塞版本）" << std::endl;
    
    // 【关键】把监听socket设置为非阻塞
    setnoblocking(sockfd);

    //5.循环等待处理
    while(true){
         // 4. 非阻塞 accept
        // 没有连接会立刻返回 -1，不会卡住
        int client_fd = accept(sockfd, NULL, NULL);
        
        if(client_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                continue;
            }
            perror("accept");
            break;
        }


        printf("Client connected\n");

        char buffer[1024] = {0};

        //非阻塞read
        ssize_t n = read(client_fd, buffer, 1023);

        if(n > 0){
            printf("Recv: %s\n", buffer);
            // 回显
            send(client_fd, buffer, n, 0);
        }

        close(client_fd);
    }

    close(sockfd);
    return ;
}

//新增设置非阻塞函数
int setnoblocking(int fd){
    //获取旧的flags
    int old_flag = fcntl(fd, F_GETFL);
    //加上非阻塞标记
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
    return new_flag;
}