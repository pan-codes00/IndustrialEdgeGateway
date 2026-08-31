#pragma once

#include "config/gateway_config.h"
#include "gateway/device_registry.h"
#include "logging/logger.h"
#include "server/device_message_processor.h"
#include "storage/sqlite_storage.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace server {

class GatewayServer {
public:
    explicit GatewayServer(config::GatewayConfig config);
    ~GatewayServer();

    GatewayServer(const GatewayServer&) = delete;
    GatewayServer& operator=(const GatewayServer&) = delete;

    bool initialize();
    int run();

    // 信号处理函数通过该接口通知服务端退出。
    static void requestStop() noexcept;

private:
    struct ClientWorker {
        std::thread thread;
        std::shared_ptr<std::atomic_bool> finished;
    };

    static bool isStopRequested() noexcept;

    bool installSignalHandler();
    bool createListeningSocket();
    void closeListeningSocket() noexcept;
    void acceptConnections();
    void startClientWorker(int clientFd, std::uint64_t sessionId);
    void handleClient(int clientFd, std::uint64_t sessionId);
    bool sendReply(
        int clientFd,
        std::string_view reply,
        std::uint64_t sessionId);
    void monitorDeviceStatus();
    void removeFinishedClientWorkers();
    void waitForClientWorkers();

    static volatile std::sig_atomic_t stopRequested_;

    config::GatewayConfig config_;
    logging::Logger logger_;
    storage::SQLiteStorage telemetryStorage_;
    gateway::DeviceRegistry registry_;
    DeviceMessageProcessor messageProcessor_;

    int listenFd_{-1};
    std::uint64_t nextSessionId_{1};
    std::vector<ClientWorker> clientWorkers_;
    std::thread monitorThread_;
    bool initialized_{false};
};

} // namespace server
