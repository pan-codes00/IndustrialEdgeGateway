#include "storage/sqlite_storage.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

bool parseLimit(
        const std::string& text,
        std::size_t& limit)
{
    try{
        std::size_t processed = 0;
        const unsigned long long value =
            std::stoull(text,&processed);

        if(processed != text.size() || value ==0 
                || value >
                static_cast<unsigned long long>(
                    std::numeric_limits<std::size_t>::max())){
            return false;
        }
        limit = static_cast<std::size_t>(value);
        return true;
    }
    catch(...){
        return false;
    }
}

std::string formatTimestemp(
        std::int64_t timestamp)
{
    const std::time_t timeValue =
        static_cast<std::time_t>(timestamp);
    std::tm localTime{};

    //linux线程安全版本的本地时间转换函数
    if(localtime_r(
                &timeValue,
                &localTime) == nullptr){
        //转换失败时保留原始时间戳
        return std::to_string(timestamp);
    }
    std::ostringstream output;

    output << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S");
    return output.str();

}

}

int main(int argc,char* argv[])
{
    if(argc != 3 && argc !=4){
        std::cerr
            << "用法："
            <<argv[0]
            <<" <数据库路径> <设备ID> [查询条数]\n";
        return 1;
    }

    const std::string databasePath =argv[1];
    const std::string deviceId = argv[2];

    std::size_t limit = 10;

    if(argc == 4 && 
            !parseLimit(argv[3],limit)){
        std::cerr << "查询条数必须是正整数\n";
        return 1;
    }

    std::error_code pathError;

    const bool databaseExists =
        std::filesystem::is_regular_file(
                databasePath,
                pathError);

    if(pathError || !databaseExists ){
        std::cerr
            <<"数据库文件不存在:"
            <<databasePath
            <<'\n';
    return 1;
    }

    storage::SQLiteStorage databaseStorage(databasePath);

    if(!databaseStorage.open()){
        return 1;
    }

    std::vector<storage::TelemetryRecord> records;

    if(!databaseStorage.queryLatestTelemetry(
                deviceId,
                limit,
                records)){
        return 1;
    }

    if(records.empty()){
        std::cout
            <<"没有找到设备数据"
            <<deviceId
            <<'\n';

        return 0;
    }

    std::cout
        <<"设备"
        << deviceId
        <<" 最近"
        << records.size()
        <<" 条数据:\n";

    for(const storage::TelemetryRecord& record: records){
        std::cout
            <<"id=" <<record.id
            <<",type=" << record.deviceType
            <<",value=" << record.value
            <<",sessionId=" << record.sessionId
            <<",createdAt=" << formatTimestemp(record.createdAt)
            <<'\n';
    }
    return 0;

}









