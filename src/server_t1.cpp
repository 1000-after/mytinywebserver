#include <iostream>
#include <sys/socket.h> //创建套接字
#include <arpa/inet.h>  //主机字网络字转换
#include <unistd.h> //close函数
#include <netinet/in.h> //网络地址结构
#include <cstring>  //清空函数

using namespace std;

int main()
{
    //1.创建套接字
    cout<<"创建电话机" <<endl;

    int server_fd = socket(AF_INET,SOCK_STREAM, 0);
    if(server_fd < 0){
        cout << "创建电话机失败" <<endl;
        close(server_fd);
        return 1;
    }

    cout <<"创建电话机成功" <<endl;

    //设置连接选项,使用快速重启服务
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));




    //2.设置地址
    struct sockaddr_in address;
    memset(&address, 0 , sizeof(address));
    address.sin_family = PF_INET;
    address.sin_addr.s_addr = INADDR_ANY;   //开放任意地址连接
    address.sin_port = htons(8080); //设置服务端口

    cout <<"电话号码设置为8080" <<endl;

    //3.绑定电话号码
    if(bind(server_fd, (struct sockaddr*)&address,sizeof(address)) < 0){
            cout << "绑定电话号码失败" <<endl;
            close(server_fd);
            return 1;
    }
     cout << "绑定电话号码成功" <<endl;

     //4.开启监听
     if(listen(server_fd, 3) < 0){  //最多监听三个联通
     cout <<"监听失败" <<endl;
    close(server_fd);
    return 1;
     }
     cout <<"电话已开启响铃，等待来电" <<endl;

     //5.等待并处理连接
     while(true)
     {
        cout << "等待客户连接" <<endl;

        //创建客户地址结构体
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        //6.接电话
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if(client_fd < 0){
            cout <<"接电话失败" <<endl;
            continue;
        }

        //获取客户端地址ip
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        cout <<"接到一个电话" <<endl;
        cout <<"客户端IP :" <<client_ip << ":" << client_port << endl;
        cout <<"客户端编号" <<client_fd <<endl;

        //7.读取客户端发送的数据
        char buffer[1024]= {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if(bytes_read > 0)
        {
            cout <<"客户端说:" <<endl << buffer << endl;

        }else if(bytes_read == 0)
        {
            cout <<"客户端什么都没说就挂了" <<endl;
        }
        else{
            cout <<"数据读取失败" <<endl;
        }

        //8.发送简单回应
        const char* response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 25\r\n"
            "\r\n"
            "Hello,我是你的web服务器";
            
        write(client_fd, response, strlen(response));
        cout <<"我回复了" <<response;
        
        //9.挂电话（关闭连接）
        close(client_fd);
        cout << "电话挂断" <<endl;


     }

     close(server_fd);
     return 0;
    
}