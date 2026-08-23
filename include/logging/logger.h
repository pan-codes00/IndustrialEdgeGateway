#pragma once

#include <fstream>
#include <mutex>
#include <string>


namespace logging{

//日志的重要程度
enum class LogLevel{
    Info,
    Warning,
    Error
};

class Logger{
public:
    explicit Logger(const std::string& filePath);
    ~Logger();

    //日志对象不能复制
    Logger(const Logger&)=delete;
    Logger& operator=(const Logger&) = delete;

    //打开日志文件
    bool open();

    //三种常用日志接口
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    //真正执行日志写入的内部函数
    void write(
        LogLevel level,
        const std::string& message);

    //把日志等级转换成文字
    static std::string levelToText(LogLevel level);
    
    //获取当前时间字符串
    static std::string currentTime();

    std::string filePath_;
    std::ofstream logFile_;

    //防止多个客户端线程同时写日志造成错乱
    std::mutex mutex_;
};

}//namespace logging



