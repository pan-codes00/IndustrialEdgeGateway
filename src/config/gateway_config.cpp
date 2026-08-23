#include "config/gateway_config.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {

//把纯数字字符串转换成大于0的size_t
bool parsePositiveNumber(
        const std::string& text,
        std::size_t& number)
{
    if(text.empty()){
        return false;
    }

    //出现非数字字符串就返回失败
    if(text.find_first_not_of("0123456789")
            !=std::string::npos){
        return false;
    }

    try{
        const unsigned long long value = std::stoull(text);
        if(value == 0 || value > 
                std::numeric_limits<std::size_t>::max()){
            return false;
        }
        number = static_cast<std::size_t>(value);
    }
    catch(...){
        return false;
    }

    return true;
}
}//namespace
 
namespace config{

bool loadGatewayConfig(
        const std::string& configPath,
        GatewayConfig& config)
{
    //根据传入的路径打开配置文件
    std::ifstream inputFile(configPath);

    //is_open() 判断文件是否打开
    if(!inputFile.is_open()){
        std::cerr
            <<"配置文件打开失败: "
            <<configPath
            <<'\n';
        return false;
    }
    std::cout
        <<"配置文件打开成功: "
        <<configPath
        <<'\n';

    std::string line;
    std::size_t lineNumber = 0;

    while(std::getline(inputFile,line)){
        ++lineNumber;

        //跳过空行以 # 开头的注释
        if(line.empty() || line[0] == '#'){
            continue;
        }
        //查找等号位置
        const std::size_t separatorPosition = line.find('=');

        //npos 表示没有找到等号
        if(separatorPosition == std::string::npos){
            std::cerr
                <<"配置文件第 "
                <<lineNumber
                <<" 行缺少等号\n";
            return false;
        }
        //等号左边是配置名 
        const std::string key=
            line.substr(0,separatorPosition);
        const std::string value = 
            line.substr(separatorPosition +1);
        if(key.empty() || value.empty()){
            std::cerr
                <<"配置文件第 "
                <<lineNumber
                <<" 行格式错误\n";
            return false;
        }
        std::cout
        <<"读取配置: "
        <<key
        <<" = "
        <<value
        <<" \n";
        //解析端口=============================================
        if(key == "port"){
            std::size_t portNumber = 0;
            //把字符串形式 "8888" 转换成数字
            if(!parsePositiveNumber(value,portNumber)){
                std::cerr
                    <<"第 "
                    <<lineNumber
                    <<" 行端口不是有效的正整数\n";
                return false;
            }
            //TCP/UDP 端口号最大为65535
            if(portNumber > 65535){
                std::cerr
                    <<"第 "
                    <<lineNumber
                    <<" 行端口超过65535\n";
                return false;
            }
            //检查通过后，保存到配置结构体
            config.port = static_cast<std::uint16_t>(portNumber);
            std::cout
                <<"端口号: "
                <<config.port
                <<" \n";
            continue;
        }
        
        //解析超时=========================================
        if(key == "timeout_seconds"){
            std::size_t timeoutNumber = 0;

            if(!parsePositiveNumber(value,timeoutNumber)){
                std::cerr
                    <<"第 "
                    <<lineNumber
                    <<" 行超时时间不是有效的正整数\n";
                return false;
            }
            config.timeoutSeconds = timeoutNumber;
            std::cout
                <<"设备超时时间: "
                <<config.timeoutSeconds
                <<" 秒\n";
            continue;
        }

        //解析路径===============================================================
        if(key == "database_path"){
            //数据库路径是字符串，不需要转换成数字
            config.databasePath = value;
            std::cout
                <<"数据库路径: "
                <<config.databasePath
                <<"\n";
            continue;
        }

        //解析缓冲区的最大长度===============================================================
        if(key == "max_pending_buffer_size"){
            std::size_t bufferSize = 0;

            if(!parsePositiveNumber(value,bufferSize)){
                std::cerr
                    <<"第 "
                    <<lineNumber
                    <<" 行缓冲区大小不是有效的正整数\n";
                return false;
            }
            config.maxPendingBufferSize = bufferSize;
            std::cout
                <<"最大接收缓冲区: "
                <<config.maxPendingBufferSize
                <<" \n";
            continue;
        }
        //解析日志路径==================================
        if(key == "log_path"){
            config.logPath = value;
            
            std::cout
                <<"日志路径: "
                <<config.logPath
                <<'\n';
            continue;
        }

        //四个合法位置都没有匹配到
        std::cerr
            <<"配置文件第  "
            <<lineNumber
            <<" 行存在未知配置项\n";
        return false;


    }
    std::cout<<"配置文件读取完成\n";
    return true;

}

}

