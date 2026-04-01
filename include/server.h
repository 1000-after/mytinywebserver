//实现基础的网络连接
#ifndef SERVER_H
#define SERVER_H

//服务端主逻辑（socket + bind + listen + accept）
#include <sys/types.h>
void runServer(uint16_t ports);


//新增设置非阻塞函数
int setnoblocking(int fd);


#endif