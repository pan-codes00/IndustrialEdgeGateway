#include "server/device_message_processor.h"

#include "protocol/device_protocol.h"

#include <string>

namespace {

constexpr std::string_view kOkReply = "OK\n";
constexpr std::string_view kErrorReply = "ERROR\n";
constexpr std::string_view kIdErrorReply = "ID_ERROR\n";
constexpr std::string_view kSessionReplacedReply =
    "SESSION_REPLACED\n";
constexpr std::string_view kStorageErrorReply =
    "STORAGE_ERROR\n";

} // namespace

namespace server {

DeviceMessageProcessor::DeviceMessageProcessor(
    gateway::DeviceRegistry& registry,
    storage::SQLiteStorage& telemetryStorage,
    logging::Logger& logger)
    : registry_(registry),
      telemetryStorage_(telemetryStorage),
      logger_(logger)
{
}

MessageProcessResult
DeviceMessageProcessor::processCompleteMessage(
    const std::string& completeMessage,
    std::string& connectedDeviceId,
    std::uint64_t sessionId)
{
    protocol::DeviceData data;
    if (!protocol::parseDeviceData(completeMessage, data)) {
        logger_.warning(
            "设备数据格式有误,sessionId=" +
            std::to_string(sessionId) +
            ",data:" +
            completeMessage);
        return {kErrorReply, MessageAction::Continue};
    }

    if (connectedDeviceId.empty()) {
        connectedDeviceId = data.deviceId;
    } else if (connectedDeviceId != data.deviceId) {
        logger_.warning(
            "同一连接尝试更换设备ID，sessionId=" +
            std::to_string(sessionId) +
            ",originalDeviceId=" +
            connectedDeviceId +
            ",newDeviceId=" +
            data.deviceId);
        return {kIdErrorReply, MessageAction::Continue};
    }

    const gateway::UpdateResult updateResult =
        registry_.update(data, sessionId);

    if (updateResult ==
        gateway::UpdateResult::ReplacedByNewerSession) {
        logger_.warning(
            "旧会话退出，无法修改设备状态，sessionId=" +
            std::to_string(sessionId));
        return {
            kSessionReplacedReply,
            MessageAction::CloseConnection
        };
    }

    if (!telemetryStorage_.saveTelemetry(data, sessionId)) {
        logger_.error(
            "设备数据保存失败,deviceId=" +
            data.deviceId +
            ",sessionId=" +
            std::to_string(sessionId));
        return {kStorageErrorReply, MessageAction::Continue};
    }

    return {kOkReply, MessageAction::Continue};
}

} // namespace server
