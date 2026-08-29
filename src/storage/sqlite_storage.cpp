#include "storage/sqlite_storage.h"

#include <iostream>
#include <filesystem>
#include <system_error>
#include <limits>
#include <utility>
#include <chrono>

namespace storage{

//构造对象时保存数据库文件路径
SQLiteStorage::SQLiteStorage(
        const std::string& databasePath):databasePath_(databasePath)
{
}

//对象销毁时自动关闭数据库        
SQLiteStorage::~SQLiteStorage()
{
    if(database_ != nullptr){
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

    //打开数据库==================================================================================
bool SQLiteStorage::open()
{
    //已经打开时，不重新打开
    if(database_ != nullptr)
    {
        return true;
    }
    const std::filesystem::path databasePath(databasePath_);
    const std::filesystem::path parentPath =
        databasePath.parent_path();
    if(!parentPath.empty()){
        std::error_code error;

        std::filesystem::create_directories(parentPath,error);
        
        if(error){
            std::cerr
                <<"创建数据库目录失败"
                <<parentPath
                <<","
                <<error.message()
                <<'\n';
            return false;
        }
    }
        
    const int result = sqlite3_open(databasePath_.c_str(),&database_);

    if(result != SQLITE_OK){
        std::cerr<<"打开数据库失败: ";
        if(database_ != nullptr){    
            std::cerr<<sqlite3_errmsg(database_);
            sqlite3_close(database_);
            database_ = nullptr;
        }
        else{
            std::cerr << sqlite3_errstr(result);
        }
        std::cerr<<'\n';
        return false;
    }
   //设置忙等待超时 
    const int timeoutResult =
        sqlite3_busy_timeout(database_,3000);

    if(timeoutResult != SQLITE_OK){
        std::cerr
            <<"设置数据库忙等待超时失败:"
            <<sqlite3_errmsg(database_)
            <<'\n';
    
    sqlite3_close(database_);
    database_ = nullptr;
    return false;
    }

    std::cout<<"数据库打开成功: "
        <<databasePath_
        <<'\n';
    return true;
}

//执行不需要返回查询结果的SQL
bool SQLiteStorage::execute(const char* sql)
{
    //没有打开数据库时不能执行SQL
    if(database_ == nullptr)
    {
        std::cerr <<"数据库尚未打开\n";
        return false;
    }
    //同一时间只能允许一个线程操作数据库
    std::lock_guard<std::mutex> lock(mutex_);
    
    char* errorMessage = nullptr;
    //执行语句
    const int result = sqlite3_exec(
        database_,
        sql,
        nullptr,
        nullptr,
        &errorMessage);
    if(result !=SQLITE_OK){
        std::cerr<<"SQL执行失败: ";

        if(errorMessage !=nullptr){
            std::cerr<<errorMessage;
            sqlite3_free(errorMessage);
        }
        else{
            std::cerr<<sqlite3_errmsg(database_);
        }
        std::cerr<<'\n';
        return false;
    }

    return true;
}

//检查或创建telemetry表================================================
bool SQLiteStorage::initialize()
{
    //wal模式允许查询和写入更好地并发执行
    if(!execute("PRAGMA journal_mode = WAL;")){
        return false;
    }
    //降低同步开销，同时保留较好的数据安全性
    if(!execute("PRAGMA synchronous = NORMAL;")){
        return false;
    }

    const char* createTableSql = R"(
        CREATE TABLE IF NOT EXISTS telemetry(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            data_type TEXT NOT NULL,
            value REAL NOT NULL,
            session_id INTEGER NOT NULL,
            created_at INTEGER NOT NULL DEFAULT (unixepoch())
        );
    )";
    if(!execute(createTableSql)){
        return false;
    }
    //增加复合索引
    const char* createIndexSql = R"(
        CREATE INDEX IF NOT EXISTS
            idx_telemetry_device_id_id
        ON telemetry(
            device_id,
            id DESC
        );
    )";
    if(!execute(createIndexSql)){
        return false;
    }

    std::cout<<"数据表检查或创建成功\n";
    return true;
}

//保存一条设备上报数据================================================================
bool SQLiteStorage::saveTelemetry(
        const protocol::DeviceData& data,
        std::uint64_t sessionId)
{
    //没有打开数据库时不能执行SQL
    if(database_ == nullptr)
    {
        std::cerr <<"数据库尚未打开\n";
        return false;
    }
    //生成当前unix时间戳
    const auto createdAt =
        std::chrono::duration_cast<
            std::chrono::seconds>(
            std::chrono::system_clock::now()
            .time_since_epoch())
            .count();
    //同一时间只能允许一个线程操作数据库
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* insertSql = R"(
        insert into telemetry(
            device_id,
            data_type,
            value,
            session_id,
            created_at
        )
        values(
        ?,?,?,?,?);
    )";
    sqlite3_stmt* statement = nullptr;
    //把SQL文本编译为SQLite可执行的预处理语句
    int result = sqlite3_prepare_v2(
        database_,
        insertSql,
        -1,
        &statement,
        nullptr);
    if(result != SQLITE_OK){
        std::cerr
            <<"准备插入语句失败: "
            <<sqlite3_errmsg(database_)
            <<'\n';
        return false;
    }
    //第1个问号绑定设备编号
    result = sqlite3_bind_text(statement,1,data.deviceId.c_str(),-1,SQLITE_TRANSIENT);
    //第2个问号绑定数据类型
    if(result == SQLITE_OK){
        result =sqlite3_bind_text(statement,2,data.deviceType.c_str(),-1,SQLITE_TRANSIENT);
    }
    //第3个问号绑定测量值
    if(result == SQLITE_OK){
        result =sqlite3_bind_double(statement,3,data.value);
    }
    //第4个问号绑定会话编号
    if(result == SQLITE_OK){
        result =sqlite3_bind_int64(statement,4,static_cast<sqlite3_int64>(sessionId));
    }
    //第5个问号绑定创建时间
    if(result == SQLITE_OK){
        result =sqlite3_bind_int64(statement,5,static_cast<sqlite3_int64>(createdAt));
    }
    if(result != SQLITE_OK){
        std::cerr
            <<"绑定设备数据失败: "
            <<sqlite3_errmsg(database_)
            <<'\n';
        sqlite3_finalize(statement);
        return false;
    }
    //真正执行插入操作=======================
    result = sqlite3_step(statement);

    if(result != SQLITE_DONE){
        std::cerr
            <<"设备插入失败: "
            <<sqlite3_errmsg(database_)
            <<'\n';
        sqlite3_finalize(statement);
        return false;
    }
    //释放预处理语句
    sqlite3_finalize(statement);
    return true;
}

//数据查询===============================================================================================
bool SQLiteStorage::queryLatestTelemetry(
        const std::string& deviceId,
        std::size_t limit,
        std::vector<TelemetryRecord>& records)
{
    records.clear();

    if(database_ == nullptr){
        std::cerr << "数据库尚未打开\n";
        return false;
    }

    if(deviceId.empty() || limit ==0 ){
        std::cerr<< "查询参数无效\n";
        return false;
    }

    if(limit > static_cast<std::size_t>(
                std::numeric_limits<sqlite3_int64>::max())){
        std::cerr<< "查询数量过大\n";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const char* querySql = R"(
        SELECT
            id,
            device_id,
            data_type,
            value,
            session_id,
            created_at
        FROM telemetry
        WHERE device_id = ?
        ORDER BY id DESC
        LIMIT ?;
    )";

    //预处理语句句柄，拿到预处理语句指针
    sqlite3_stmt* statement = nullptr;

    int result = sqlite3_prepare_v2(
            database_,
            querySql,
            -1,
            &statement,
            nullptr);

    if(result != SQLITE_OK){
        std::cerr
            <<"准备查询失败:"
            <<sqlite3_errmsg(database_)
            <<'\n';

        return false;
    }

    //第一个问号绑定设备ID
    result = sqlite3_bind_text(
        statement,
        1,
        deviceId.c_str(),
        -1,
        SQLITE_TRANSIENT);

    //第二个问号绑定查询数量
    if(result == SQLITE_OK ){
        result = sqlite3_bind_int64(
            statement,
            2,
            static_cast<sqlite3_uint64>(limit));
    }
    
    if(result != SQLITE_OK){
        std::cerr
            <<"绑定查询参数失败:"
            <<sqlite3_errmsg(database_)
            <<'\n';

        sqlite3_finalize(statement);
        return false;
    }
    while((result = sqlite3_step(statement)) == SQLITE_ROW){
        const unsigned char* deviceIdText = sqlite3_column_text(statement,1);

        const unsigned char* deviceTypeText = sqlite3_column_text(statement,2);

        if(deviceIdText == nullptr || deviceTypeText == nullptr){
            std::cerr << "数据库记录内容无效\n";

            sqlite3_finalize(statement);
            records.clear();
            return false;
        }
       //把数据库字段解析成TelemetryRecord结构体，塞进vector 
        TelemetryRecord record;
        record.id = static_cast<std::int64_t>(
                sqlite3_column_int64(statement,0));
        record.deviceId = 
            reinterpret_cast<const char*>(deviceIdText);
        record.deviceType = 
            reinterpret_cast<const char*>(deviceTypeText);
        record.value = 
            sqlite3_column_double(statement,3);
        record.sessionId = 
            static_cast<std::uint64_t>(sqlite3_column_int64(statement,4));
        record.createdAt = 
            static_cast<std::uint64_t>(sqlite3_column_int64(statement,5));
        records.push_back(std::move(record));
    }
    if(result != SQLITE_DONE){
        std::cerr
            <<"执行查询失败:"
            <<sqlite3_errmsg(database_)
            <<'\n';
        sqlite3_finalize(statement);
        records.clear();
        return false;
    }
    sqlite3_finalize(statement);
    return true;

}

}//namespace storage




