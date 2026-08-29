#include "gateway/device_registry.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

int failureCount = 0;

void expect( bool condition,const std::string& testName)
{
    if(condition){
        std::cout
            <<"[PASS]"
            <<testName
            <<'\n';
        return ;
    }
    std::cerr
        <<"[FAIL]"
        <<testName
        <<'\n';

    ++failureCount;
}

bool valueEquals(
        const gateway::DeviceSnapshot& snapshot,
        const std::string& dataType,
        double expectedValue)
{
    const auto it =
        snapshot.values.find(dataType);
    return
        it != snapshot.values.end() &&
        std::abs(it->second - expectedValue) < 1e-9;
}


};//namespace

int main()
{
    gateway::DeviceRegistry registry;
    gateway::DeviceSnapshot snapshot;

    expect(
            !registry.getSnapshot(
                "missiing",
                snapshot),
            "unknown device has no snapshot");

    protocol::DeviceData firstData{
        "deviceA",
        "temperature",
        25.5
    };

    expect(
            registry.update(firstData,10)==
            gateway::UpdateResult::Updated,
            "first session registers devices");

    expect(
            registry.getSnapshot(
                "deviceA",
                snapshot),
            "registered device has  snapshot");

    expect(
            snapshot.online,
            "registered device is online");

    expect(
            snapshot.sessionId ==10,
            "first session owns device");

    expect(
            valueEquals(
                snapshot,
                "temperature",
                25.5),
            "first value is stored");

    protocol::DeviceData newerData{
        "deviceA",
        "temperature",
        26.0
    };

    expect(
            registry.update(newerData,11)==
            gateway::UpdateResult::Updated,
            "newer session replaces old session");

    protocol::DeviceData staleData{
        "deviceA",
        "speed",
        999.0
    };

    expect(
        registry.update(staleData,10)==
        gateway::UpdateResult::ReplacedByNewerSession,
        "old session update is registered");

    registry.getSnapshot(
            "deviceA",
            snapshot);

    expect(
            snapshot.sessionId ==11,
            "new session remain owner");

    expect(
            snapshot.values.find("speed")==
            snapshot.values.end(),
            "old session cannot modify values");

    expect(
            !registry.markOfflineIfOwner(
                "deviceA",
                10),
            "old session cannot mark offline");

    registry.getSnapshot(
            "deviceA",
            snapshot);

    expect(
            snapshot.online,
            "device remain online");
    expect(
            registry.markOfflineIfOwner(
                "deviceA",
                11),
            "current session can mark offline");

    registry.getSnapshot(
            "deviceA",
            snapshot);

    expect(
            !snapshot.online,
            "device is offline after owner exits");

    registry.update(newerData,11);

    registry.getSnapshot(
            "deviceA",
            snapshot);

    expect(
            snapshot.online,
            "new data restores online state");

    //使用0秒避免测试依赖sleep
    registry.markTimedOut(std::chrono::seconds(0));

    registry.getSnapshot(
        "deviceA",
        snapshot);

    expect(
            !snapshot.online,
            "timeout marks device offline");

    if(failureCount !=0){
        std::cerr
            <<failureCount
            <<" device registry test(s) failed\n";
            
        return 1;
    }
    std::cout
        <<"All device registry tests passed\n";
    
    return 0;
}









