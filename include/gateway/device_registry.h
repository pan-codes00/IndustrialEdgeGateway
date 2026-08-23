#pragma once

#include "protocol/device_protocol.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace gateway{

enum class UpdateResult{
    Updated,
    ReplacedByNewerSession
};

class DeviceRegistry{
public:
    UpdateResult update(
            const protocol::DeviceData& data,
            std::uint64_t sessionId);

    bool markOfflineIfOwner(
            const std::string& deviceId,
            std::uint64_t sessionId);
    
    void markTimedOut(
            std::chrono::seconds timeout);

private:
    struct DeviceStatus{
        std::string deviceId;
        std::unordered_map<std::string,double> values;
        bool online = false;
        std::chrono::steady_clock::time_point lastUpdate;
        std::uint64_t sessionId = 0;
    };
    void printAllUnlocked()const;

    std::unordered_map<std::string,DeviceStatus> devices_;
    std::mutex mutex_;

};

}
