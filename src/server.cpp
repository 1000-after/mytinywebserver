#include <iostream>
#include <sys/socket.h>//创建套机字
#include <netinet/in.h> //网络字主机字转换
#include <unistd.h>
#include <string.h>
#include "server.h"

void runServer(uint16_t ports){
    //1.创建电话机创建套接字
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        std::cout <<"创建套接字失败" << std::endl;
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
        return;
    }

    //4.监听
    listen(sockfd, 5);
    std::cout <<"开启监听" << std::endl;
    
    //5.循环等待处理
    while(true){
        int client_fd = accept(sockfd, NULL, NULL);
         printf("Client connected\n");

        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);
        printf("Recv: %s\n", buffer);

        // 回显
        send(client_fd, buffer, strlen(buffer), 0);
        close(client_fd);
    }

    close(sockfd);
    return ;
}