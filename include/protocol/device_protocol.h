#pragma once

#include <string>

namespace protocol{

struct DeviceData{
    std::string deviceId;
    std::string deviceType;
    double value = 0.0;
};

bool parseDeviceData(
    const std::string& messages,
    DeviceData& data);


}
