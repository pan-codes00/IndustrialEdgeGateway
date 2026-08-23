#pragma once

#include <cstddef>
#include <string>

namespace net{

//把指定长度的数据全部发送出去
bool sendAll(int socketFd,const char* data,std::size_t length);

bool receiveLine(
    int socketFd,
    std::string& pendingBuffer,
    std::string& reply);
}
