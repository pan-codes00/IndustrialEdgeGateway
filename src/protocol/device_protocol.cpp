#include "protocol/device_protocol.h"

#include <sstream>
#include <cmath>

namespace protocol{

//把文本解析
bool parseDeviceData(
    const std::string& messages,
    DeviceData& data)
{
    std::stringstream stream(messages);
    std::string valueText;
    //读取第一个字段
    if(!std::getline(
        stream,
        data.deviceId,
        ',')){
        return false;
    }
    //TODO：读取dataType
    if(!std::getline(
        stream,
        data.deviceType,
        ',')){
        return false;
    }
    //TODO：读取valueText
    if(!std::getline(
        stream,
        valueText)){
        return false;
    }
    if(data.deviceId.empty() ||
        data.deviceType.empty() ||
        valueText.empty()){
        return false;
    }
    //try-catch异常捕获
    try{
        std::size_t processed=0;
        //processed表示成功转化的字符数
        data.value = std::stod(valueText, &processed);
        if(processed != valueText.size() || !std::isfinite(data.value)){
            return false;
        }
    }
    catch(...){
        return false;
    }
    return true;
}


}
