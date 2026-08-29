#include "protocol/device_protocol.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failureCount =0;

void expect(bool condition, const std::string& testName)
{
    if(condition){
        std::cout
            <<"[PASS]"
            <<testName
            <<'\n';
        return ;
    }

    std::cerr
        <<"[FSIL]"
        <<testName
        <<'\n';
    ++failureCount;
}

void expectValidMessage()
{
    protocol::DeviceData data;
    const bool result =protocol::parseDeviceData("deviceA,temperature,25.5",data);
    expect(result,"valid message is accepted");
    expect(data.deviceId == "deviceA","device ID is parsed");
    expect(data.deviceType == "temperature","device type is parseed");
    expect(std::abs(data.value - 25.5)< 1e-9,"value is parseed");
}

void expectInvalidMessage(const std::string& message,const std::string& testName)
{
    protocol::DeviceData data;
    expect(!protocol::parseDeviceData(message,data),testName);
}
};//namespace

int main()
{
    expectValidMessage();
    expectInvalidMessage("deviceA,temperature","missing value is rejected");
 
    expectInvalidMessage(",temperature,25.5","empty device ID is rejected");
    expectInvalidMessage("deviceA,25.5","empty device type is rejected");
    expectInvalidMessage("deviceA,temperature","empty value is rejected");

    expectInvalidMessage("deviceA,temperature,25.5abc","trailing characters are rejected");
    expectInvalidMessage("deviceA,temperature,nan","NaN  is rejected");
    expectInvalidMessage("deviceA,temperature,ind","infinity  is rejected");

    expectInvalidMessage("deviceA,temperature,25.5,extra","extra filed is rejected");

    if(failureCount !=0){
        std::cerr
            <<failureCount
            <<" protocol test(s) failed\n";
        return 1;
    }
    std::cout
        <<"All protocol tests passed\n";
    return 0;

}








