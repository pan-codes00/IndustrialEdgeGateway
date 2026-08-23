#include "storage/sqlite_storage.h"

#include <iostream>


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

    //打开数据库
bool SQLiteStorage::open()
{
    //已经打开时，不重新打开
    if(database_ != nullptr)
    {
        return true;
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

//检查或创建telemetry表
bool SQLiteStorage::initialize()
{
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
    std::cout<<"数据表检查或创建成功\n";
    return true;
}
//保存一条设备上报数据
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
    //同一时间只能允许一个线程操作数据库
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* insertSql = R"(
        insert into telemetry(
            device_id,
            data_type,
            value,
            session_id
        )
        values(
        ?,?,?,?);
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
    if(result != SQLITE_OK){
        std::cerr
            <<"绑定设备数据失败: "
            <<sqlite3_errmsg(database_)
            <<'\n';
        sqlite3_finalize(statement);
        return false;
    }
    //真正执行插入
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

}//namespace storage




