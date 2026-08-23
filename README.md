# 工业设备边缘数据网关

基于 Linux C++17 开发的工业设备数据网关，模拟多台设备通过 TCP 长连接上报遥测数据，网关完成协议解析、会话管理、数据持久化与设备状态监控。

## 技术栈

C++17、Linux、TCP Socket、多线程、SQLite3、CMake

## 项目结构

```
├── CMakeLists.txt      # CMake构建配置
├── config/             # 配置文件
│   └── gateway.conf
├── include/            # 头文件
│   ├── common/         # 网络通信工具
│   ├── protocol/       # 协议解析
│   ├── gateway/        # 设备注册管理
│   ├── storage/        # SQLite存储
│   ├── config/         # 配置加载
│   └── logging/        # 日志系统
└── src/                # 源代码
    ├── server.cpp      # 网关服务端
    └── client.cpp      # 设备模拟器
```

## 核心功能

- **TCP粘包拆包**：换行符消息边界 + pendingBuffer 接收缓存，4096字节缓存上限
- **一连接一线程**：每个客户端独立线程处理，支持多设备并发接入
- **设备会话接管**：自增 sessionId 区分新旧连接，新连接登录同设备ID时旧连接自动失效
- **线程安全**：DeviceRegistry、SQLiteStorage、Logger 内部 mutex + lock_guard 加锁
- **SQLite持久化**：预处理语句 + 参数绑定防注入，RAII管理数据库连接
- **SIGINT优雅退出**：信号处理 + 线程 join 安全退出
- **配置文件**：key=value 格式解析，端口范围校验
- **三级日志**：Info/Warning/Error，线程安全，文件落盘

## 编译运行

```bash
# 安装依赖
sudo apt install cmake libsqlite3-dev

# 编译
mkdir build && cd build
cmake ..
make

# 运行服务端
./gateway_server

# 运行设备模拟器（另开终端）
./device_simulator
```

## 协议格式

设备上报数据格式（CSV，换行符结尾）：

```
deviceId,dataType,value
```

示例：

```
device001,temperature,25.5
device001,speed,1000.0
```

服务端应答码：

| 应答码 | 说明 |
|--------|------|
| `OK` | 数据接收成功 |
| `ERROR` | 数据格式错误 |
| `ID_ERROR` | 同一连接尝试更换设备ID |
| `SESSION_REPLACED` | 旧会话已被新连接替代 |
| `FRAME_TOO_LARGE` | 数据帧超过缓存上限 |
| `STORAGE_ERROR` | 数据库保存失败 |
