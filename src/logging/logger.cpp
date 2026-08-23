#include "logging/logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>


namespace logging{

Logger::Logger(const std::string& filePath):filePath_(filePath)
{
}

Logger::~Logger()
{
    if(logFile_.is_open()){
        logFile_.close();
    }
}

bool Logger::open()
{
    std::lock_guard<std::mutex> lock(mutex_);

    //取得日志文件所在目录，例如 logs/gateway.lpg 的 logs
    const std::filesystem::path logPath(filePath_);
    const std::filesystem::path parentPath=
        logPath.parent_path();

    //如果不存在就自动创建
    if(!parentPath.empty()){
        std::error_code error;

        std::filesystem::create_directories(parentPath,error);

        if(error){
            std::cerr
                <<"日志目录创建失败: "
                <<error.message()
                <<'\n';

            return false;
        }
    }

    //app 表示追加写入，不覆盖以前的日志
    logFile_.open(filePath_,std::ios::app);

    if(!logFile_.is_open()){
        std::cerr
            <<"日志打开失败: "
            <<filePath_
            <<'\n';

        return false;
        
    }
    return true;
}

void Logger::info(const std::string& message)
{
    write(LogLevel::Info,message);
}
void Logger::warning(const std::string& message)
{
    write(LogLevel::Warning,message);

}
void Logger::error(const std::string& message)
{    
    write(LogLevel::Error,message);
}

void Logger::write(
        LogLevel level,
        const std::string& message)
{
    //从拼接到写入全部加锁，避免多线程日志混乱
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream logText;

    logText
        <<'['
        <<currentTime()
        <<"] ["
        <<levelToText(level)
        <<"] "
        <<message;

    //ERROE输出到cerr,其它等级输出到cout
    if(level == LogLevel::Error){
        std::cerr<< logText.str() << '\n';
    }
    else{
        std::cout<< logText.str() << '\n';
    }

    if(logFile_.is_open()){
        logFile_
            << logText.str()
            <<'\n';
        //立即保存，防止程序异常退出时日志还留在缓冲区
        logFile_.flush();
    }
}

std::string Logger::levelToText(LogLevel level)
{
    switch(level){
        case LogLevel::Info:
            return "Info";
        case LogLevel::Warning:
                return "Warning";
        case LogLevel::Error:
                return "Error";
    }
    return "UNKNOWN";
}

std::string Logger::currentTime()
{
    const auto now=
        std::chrono::system_clock::now();

    const std::time_t currentTimeValue =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{}; 

    //linux线程安全版本的本地时间转换函数
    localtime_r(
            &currentTimeValue,
            &localTime);

    std::ostringstream timeText;

    timeText << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S");

    return timeText.str();
}

}//namespace logging

