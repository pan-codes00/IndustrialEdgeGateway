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
#include <cstdint>

namespace {

bool parsePort(const std::string& text,
        std::uint16_t& port)
{
    if(text.empty()){
        return false;
    }

    try{
        std::size_t processed =0;

        const unsigned long value =
            std::stoul(text,&processed);

        if(processed != text.size() || 
                value ==0 ||
                value > 65535){
            return false;
        }

        port = static_cast<std::uint16_t>(value);

        return true;
    }
    catch(...){
        return false;
    }
}

};//namespace

int main(int argc,char *argv[])
{
    if(argc > 4){
        std::cerr
            << "用法: "
            << argv[0]
            <<" [设备ID] [服务器IP] [端口]\n";
    }
    
    std::string deviceId  = "device001";
    std::string serverIp = "127.0.0.1";
    std::uint16_t port  = 8888;

    if(argc >= 2){
        deviceId = argv[1];
    }
    if(argc >= 3){
        serverIp = argv[2];
    }
    if(argc == 4 &&
        !parsePort(argv[3],port)){
        std::cerr
            <<"端口必须是1到65535之间的整数\n";
        
        return 1;
    }

    if(deviceId.empty() || 
            serverIp.empty()){
        std::cerr
            <<"设备ID和服务器IP不能为空\n";
       return 1; 
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
    serverAddr.sin_port = htons(port);
    const int addressResult = 
        inet_pton(
            AF_INET,
            serverIp.c_str(),
            &serverAddr.sin_addr);
    
    if(addressResult != 1){
        std::cerr
            <<"服务器IPV4地址无效:"
            <<serverIp
            <<'\n';

        close(socketFd);
        return 1;
    }

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
        //拼接信息发送
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
