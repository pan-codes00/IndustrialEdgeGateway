#include "server/device_message_processor.h"

#include "gateway/device_registry.h"
#include "logging/logger.h"
#include "protocol/device_protocol.h"
#include "storage/sqlite_storage.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message)
{
    if (condition) {
        return true;
    }

    std::cerr << "测试失败: " << message << '\n';
    return false;
}

bool testValidMessageIsSaved()
{
    logging::Logger logger("");
    storage::SQLiteStorage storage(":memory:");
    gateway::DeviceRegistry registry;

    if (!expect(
            storage.open() && storage.initialize(),
            "内存数据库应该初始化成功")) {
        return false;
    }

    server::DeviceMessageProcessor processor(
        registry,
        storage,
        logger);
    std::string connectedDeviceId;

    const server::MessageProcessResult result =
        processor.processCompleteMessage(
            "device-1,temperature,23.5",
            connectedDeviceId,
            1);

    std::vector<storage::TelemetryRecord> records;
    const bool querySucceeded =
        storage.queryLatestTelemetry("device-1", 1, records);

    return expect(result.reply == "OK\n", "有效消息应该回复OK") &&
        expect(
            result.action == server::MessageAction::Continue,
            "有效消息后连接应该保持") &&
        expect(
            connectedDeviceId == "device-1",
            "首条有效消息应该绑定设备ID") &&
        expect(querySucceeded, "应该能够查询已保存的数据") &&
        expect(records.size() == 1, "应该只保存一条数据") &&
        expect(
            !records.empty() &&
                records.front().deviceType == "temperature" &&
                std::abs(records.front().value - 23.5) < 0.0001,
            "保存的数据内容应该正确");
}

bool testInvalidMessageIsRejected()
{
    logging::Logger logger("");
    storage::SQLiteStorage storage(":memory:");
    gateway::DeviceRegistry registry;
    server::DeviceMessageProcessor processor(
        registry,
        storage,
        logger);
    std::string connectedDeviceId;

    const server::MessageProcessResult result =
        processor.processCompleteMessage(
            "invalid-message",
            connectedDeviceId,
            2);

    return expect(result.reply == "ERROR\n", "非法格式应该回复ERROR") &&
        expect(
            result.action == server::MessageAction::Continue,
            "非法格式不应该主动断开连接") &&
        expect(
            connectedDeviceId.empty(),
            "非法消息不应该绑定设备ID");
}

bool testDeviceIdCannotChange()
{
    logging::Logger logger("");
    storage::SQLiteStorage storage(":memory:");
    gateway::DeviceRegistry registry;
    server::DeviceMessageProcessor processor(
        registry,
        storage,
        logger);
    std::string connectedDeviceId = "device-1";

    const server::MessageProcessResult result =
        processor.processCompleteMessage(
            "device-2,temperature,20",
            connectedDeviceId,
            3);

    return expect(
               result.reply == "ID_ERROR\n",
               "同一连接更换设备ID应该回复ID_ERROR") &&
        expect(
            result.action == server::MessageAction::Continue,
            "ID错误不应该主动断开连接") &&
        expect(
            connectedDeviceId == "device-1",
            "ID错误后应该保留原设备ID");
}

bool testOlderSessionIsClosed()
{
    logging::Logger logger("");
    storage::SQLiteStorage storage(":memory:");
    gateway::DeviceRegistry registry;
    server::DeviceMessageProcessor processor(
        registry,
        storage,
        logger);

    protocol::DeviceData newerData {
        "device-1",
        "temperature",
        30.0
    };
    registry.update(newerData, 10);

    std::string connectedDeviceId = "device-1";
    const server::MessageProcessResult result =
        processor.processCompleteMessage(
            "device-1,temperature,20",
            connectedDeviceId,
            9);

    return expect(
               result.reply == "SESSION_REPLACED\n",
               "旧会话应该收到SESSION_REPLACED") &&
        expect(
            result.action == server::MessageAction::CloseConnection,
            "旧会话应该被关闭");
}

bool testStorageFailureIsReported()
{
    logging::Logger logger("");
    storage::SQLiteStorage unopenedStorage(":memory:");
    gateway::DeviceRegistry registry;
    server::DeviceMessageProcessor processor(
        registry,
        unopenedStorage,
        logger);
    std::string connectedDeviceId;

    const server::MessageProcessResult result =
        processor.processCompleteMessage(
            "device-1,temperature,20",
            connectedDeviceId,
            4);

    return expect(
               result.reply == "STORAGE_ERROR\n",
               "保存失败应该回复STORAGE_ERROR") &&
        expect(
            result.action == server::MessageAction::Continue,
            "一次保存失败不应该主动断开连接");
}

} // namespace

int main()
{
    const bool passed =
        testValidMessageIsSaved() &&
        testInvalidMessageIsRejected() &&
        testDeviceIdCannotChange() &&
        testOlderSessionIsClosed() &&
        testStorageFailureIsReported();

    if (!passed) {
        return 1;
    }

    std::cout << "device_message_processor_tests: 全部通过\n";
    return 0;
}
