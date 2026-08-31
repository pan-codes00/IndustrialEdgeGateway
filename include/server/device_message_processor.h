#pragma once

#include "gateway/device_registry.h"
#include "logging/logger.h"
#include "storage/sqlite_storage.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace server {

enum class MessageAction {
    Continue,
    CloseConnection
};

struct MessageProcessResult {
    std::string_view reply;
    MessageAction action{MessageAction::Continue};
};

// 处理一条已经去除换行符的完整设备消息。
class DeviceMessageProcessor {
public:
    DeviceMessageProcessor(
        gateway::DeviceRegistry& registry,
        storage::SQLiteStorage& telemetryStorage,
        logging::Logger& logger);

    MessageProcessResult processCompleteMessage(
        const std::string& completeMessage,
        std::string& connectedDeviceId,
        std::uint64_t sessionId);

private:
    gateway::DeviceRegistry& registry_;
    storage::SQLiteStorage& telemetryStorage_;
    logging::Logger& logger_;
};

} // namespace server
