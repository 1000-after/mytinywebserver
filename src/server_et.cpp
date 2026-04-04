#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include "server.h" 

#define MAX_EVENTS 1024
#define BUF_SIZE 1024

int setnonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_flag | O_NONBLOCK);
    return old_flag;
}

// ET 模式
static void epollAddFd(int epoll_fd, int fd)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // ✅ ET
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

void runServer(uint16_t ports)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ports);
    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 100);

    int epoll_fd = epoll_create1(0);
    epollAddFd(epoll_fd, sockfd);

    epoll_event events[MAX_EVENTS];
    printf("ET 模式服务器已启动\n");

    while (1)
    {
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < nready; i++)
        {
            int fd = events[i].data.fd;

            if (fd == sockfd)
            {
                // ==========================
                // ✅ ET 铁律 1：监听fd必须循环accept
                // ==========================
                while (1)
                {
                    sockaddr_in cli_addr;
                    socklen_t len = sizeof(cli_addr);
                    int cfd = accept(sockfd, (struct sockaddr*)&cli_addr, &len);

                    if (cfd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // 读完了
                        else
                            break;
                    }
                    epollAddFd(epoll_fd, cfd);
                }
            }
            else
            {
                // ==========================
                // ✅ ET 铁律 2：必须循环读空
                // ==========================
                char buf[BUF_SIZE];
                ssize_t n;

                while (1)
                {
                    n = read(fd, buf, BUF_SIZE);
                    if (n > 0)
                    {
                        write(fd, buf, n);
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        else
                            break;
                    }
                }

                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }
    }
    close(epoll_fd);
    close(sockfd);
}