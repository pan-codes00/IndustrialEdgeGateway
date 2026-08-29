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

struct DeviceSnapshot{
    std::string deviceId;
    std::unordered_map<std::string, double> values;
    bool online = false;
    std::uint64_t sessionId =0;
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

    bool getSnapshot(
            const std::string& deviceId,
            DeviceSnapshot& snapshot) const;
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
    mutable std::mutex mutex_;

};

}
