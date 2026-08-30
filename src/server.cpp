#include "config/gateway_config.h"
#include "server/gateway_server.h"

#include <iostream>
#include <string>
#include <utility>

int main(int argc, char* argv[])
{
    if (argc > 2) {
        std::cerr
            << "用法："
            << argv[0]
            << " [配置文件路径]\n";
        return 1;
    }

    const std::string configPath =
        argc == 2 ? argv[1] : "config/gateway.conf";

    config::GatewayConfig gatewayConfig;
    if (!config::loadGatewayConfig(configPath, gatewayConfig)) {
        std::cerr << "网关配置加载失败，服务器退出\n";
        return 1;
    }

    server::GatewayServer gatewayServer(std::move(gatewayConfig));
    if (!gatewayServer.initialize()) {
        return 1;
    }

    return gatewayServer.run();
}
