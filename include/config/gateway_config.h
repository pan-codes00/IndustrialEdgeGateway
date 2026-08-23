#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace config{

struct GatewayConfig{
    //服务器监听端口
    std::uint16_t port{8888};

    //设置离线超时秒数
    std::size_t timeoutSeconds{5};

    //SQLite数据库文件路径
    std::string databasePath{"data/gateway.db"};

    //单个客户端最大接收缓存
    std::size_t maxPendingBufferSize{4096};

    std::string logPath{"logs/gateway.log"};
};
    //从配置文件读取参数
    bool loadGatewayConfig(
            const std::string& configPath,
            GatewayConfig& config);
}//namespace config

