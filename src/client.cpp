#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include "common/socket_utils.h"
#include <iostream>
#include <string>
#include <cerrno>


int main(int argc,char *argv[])
{
    std::string deviceId  = "device001";
    if(argc >= 2){
        deviceId = argv[1];
    }

    //1.创建Socket
    int socketFd = socket(AF_INET,SOCK_STREAM,0);
    if(socketFd == -1){
        std::cerr << "sock创建失败\n";
        return 1;
    }
    
    //2.设置服务器地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddr.sin_addr
            );

    //3.连接服务器
    if(connect(
        socketFd,
        reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr)
                )==-1){
        std::cerr << "连接服务器失败\n" ;
        close(socketFd);
        return 1;
    }
    std::cout<< "连接服务成功\n";

    //4.发送模拟设备数据
    std::string replyBuffer;
    for(int i =0;i<5;++i){
        std::string dataType;
        double value = 0.0;
        if(i%2==0){
            dataType ="temperature";
            value = 25.5+i*0.2;
            //deviceId = "device001";
        }
        else{
            dataType ="speed";
            value = 1000.0+i*10;
            //deviceId = "device002";
        }
        std::string message= 
            deviceId+","
            + dataType+","
            +std::to_string(value)
            +"\n";
        if(!net::sendAll(
                socketFd,
                message.c_str(),
                message.size())){
            perror("send");
            break;
        }
        std::cout <<"发送数据："<<message;
        //5.接受服务器回复
        std::string reply;
        if(!net::receiveLine(socketFd,replyBuffer,reply)){
            std::cout<<"服务器断开连接\n";
                break;
        }
        std::cout << "服务器回复:"<<reply<<'\n';
        //区分不同协议回复
        if(reply == "SESSION_REPLACED"){
            std::cout<<"当前连接已被新会话替代，停止发送\n";
            break;
        }   
        else if(reply == "ID_ERROR"){
            std::cout<<"同一连接不能更换设备ID\n";
            break;
        }   
        else if(reply == "FRAME_TOO_LARGE"){
            std::cout<<"发送的数据帧超过限制\n";
            break;
        }   
        else if(reply == "ERROR"){
            std::cout<<"本次数据格式错误，继续发送下一条\n";
        }   
        else if(reply == "STORAGE_ERROR"){
            std::cout<<"服务端保存数据失败，停止发送\n";
            break;
        }   
        else if(reply != "OK"){
            std::cout<<"收到未知服务器回复，停止发送\n";
            break;
        }   

        //等待一秒
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
                );
    }
    //6.关闭Socket
    close(socketFd);
   
    return 0;
}
