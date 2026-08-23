#include "common/socket_utils.h"

#include <sys/socket.h>
#include <cerrno>

namespace net{

//把指定长度数据全部发送出去
bool sendAll(int socketFd,const char* data,std::size_t length)
{
    std::size_t totalSent = 0;

    while(totalSent < length){
        ssize_t sentBytes = send(
                socketFd,
                data+totalSent,
                length - totalSent,
                MSG_NOSIGNAL);
        if(sentBytes >0){
            totalSent +=
                static_cast<std::size_t>(sentBytes);
        }
        else if(sentBytes == -1 && errno == EINTR){
            //被信号临时中断，继续发送
            continue;
        }
        else{
            return false;
        }
    }
    return true;
}

//客户端接收函数
bool receiveLine(
    int socketFd,
    std::string& pendingBuffer,
    std::string& reply)
{
    while(true){
        std::size_t newlinePosition=pendingBuffer.find('\n');
        if(newlinePosition !=std::string::npos){
            reply = pendingBuffer.substr(0,newlinePosition);
            pendingBuffer.erase(0,newlinePosition+1);
            return true;
        }
        char buffer[256];
        ssize_t receivedBytes = recv(socketFd,buffer,sizeof(buffer),0);
        if(receivedBytes >0){
            pendingBuffer.append(buffer,static_cast<std::size_t>(receivedBytes));
        }
        else if(receivedBytes == -1 && errno == EINTR){
            continue;
        }
        else{
            return false;
        }
    }
}
}
