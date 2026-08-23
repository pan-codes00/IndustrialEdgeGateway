#include "gateway/device_registry.h"

#include <iostream>

namespace gateway{

//更新设备数据
UpdateResult DeviceRegistry::update(
        const protocol::DeviceData& data,
        std::uint64_t sessionId)
{
    //锁的范围尽量小 
    std::lock_guard<std::mutex> lock(mutex_);
    
    DeviceStatus& status = 
        devices_[data.deviceId];
    //设备已经被编号更大的新连接接管
    if(status.sessionId > sessionId){
        return UpdateResult::ReplacedByNewerSession;
    }
    //首次连接，当前连接或新连接接管
    status.sessionId = sessionId;
    status.deviceId=data.deviceId;
    status.online = true;
    status.values[data.deviceType]=
        data.value;
    status.lastUpdate = 
        std::chrono::steady_clock::now();
    printAllUnlocked();
    return UpdateResult::Updated;
}

//客户端断开时调用
bool DeviceRegistry::markOfflineIfOwner(
            const std::string& deviceId,
            std::uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(deviceId);
    if(it == devices_.end() ||
            it->second.sessionId != sessionId){
        return false;
    }
    it->second.online = false;
    printAllUnlocked();
    return true;

} 
//客户端超时离线标记
void DeviceRegistry::markTimedOut(
            std::chrono::seconds timeout)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    const auto now=std::chrono::steady_clock::now();
    bool changed =false;

    for(auto& [deviceId,status] :devices_){
        if(status.online && now - status.lastUpdate >= timeout){
            
            status.online = false;
            changed = true;
            
            std::cout 
                <<"设备"<<deviceId
                <<"超时离线\n";
        }
    }
    if(changed){
        printAllUnlocked();
    }
}

//打印所有设备，会话编号，状态，和测量数据
void DeviceRegistry::printAllUnlocked()const
{
    std::cout << "-------------------当前设备状态-------------------------\n";
    
    for(const auto& [deviceId,status]:devices_){
        std::cout 
            <<"设备ID: "<<deviceId
            <<",会话ID: "<<status.sessionId
            <<",状态: "<<(status.online?"在线":"离线")
            <<'\n';

        for(const auto& [dataType,value]:status.values){      
            std::cout
                <<"  "
                <<dataType
                <<": " 
                <<value
                <<'\n';
        }
    }
    std::cout<<"设备数量: "
        <<devices_.size()
        <<'\n';
}
}


