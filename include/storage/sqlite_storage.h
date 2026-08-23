#pragma once

#include "protocol/device_protocol.h"

#include <sqlite3.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace storage{

class SQLiteStorage{
    public:
        //构造对象时保存数据库文件路径
        explicit SQLiteStorage(const std::string& databasePath);
        //对象销毁时自动关闭数据库        
        ~SQLiteStorage();
        //禁止复制数据库连接
        SQLiteStorage(const SQLiteStorage&)=delete;
        
        SQLiteStorage& operator=(const SQLiteStorage&)=delete;
        //打开数据库
        bool open();
        //检查或创建telemetry表
        bool initialize();
        //保存一条设备上报数据
        bool saveTelemetry(
                const protocol::DeviceData& data,
                std::uint64_t sessionId);

    private:
        //执行不需要返回查询结果的SQL
        bool execute(const char* sql);
        //数据库文件路径
        std::string databasePath_;
        //已经打开的SQLite数据库连接
        sqlite3* database_{nullptr};
        //防止多个客户端线程同时操作数据库
        std::mutex mutex_;
        
};

}//namespace storage


