#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <thread>
#include <functional>
#include "common/socket_utils.h"
#include "protocol/device_protocol.h"
#include "gateway/device_registry.h"
#include "storage/sqlite_storage.h"
#include "config/gateway_config.h"
#include "logging/logger.h"

#include <iostream>
#include <string>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <system_error>
#include <signal.h>
#include <sys/time.h>
#include <vector>

namespace {

std::string makeSystemErrorMessage(const std::string& operation)
{
    //立即保存errno，避免后续函数修改它
    const int errorNumber = errno;

    const std::error_code error(errorNumber,std::generic_category());

    return operation +
        "失败，errno="+
        std::to_string(errorNumber)+
        ",message"+
        error.message();
}

volatile sig_atomic_t stopRequested =0;

void handleStopSignal(int)
{
    //信号处理函数只修改标志，不输出日志
    stopRequested =1;
}

}

//检测设备是否离线函数=====================================
void monitorDeviceStatus(
        gateway::DeviceRegistry& registry,
        std::size_t timeoutSeconds)
{
    while(!stopRequested){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if(stopRequested){
            break;
        }

        registry.markTimedOut(std::chrono::seconds(timeoutSeconds));
    }
}
//对客户操作函数====================================================
void handleClient(
    int clientFd,
    std::uint64_t sessionId,
    gateway::DeviceRegistry& registry,
    storage::SQLiteStorage& telemetryStorage,
    logging::Logger& logger,
    std::size_t maxPendingBufferSize)
{
    std::string pendingBuffer;//建立接收缓存
    std::string connectDeviceId; //表示这个tcp连接属于哪台设备
    bool connectionActive = true;//增加连接运行标志

    //循环接受客户端数据
    while(connectionActive && !stopRequested){
        char buffer[1024]{};
        ssize_t receivedBytes = recv(
            clientFd,
            buffer,
            sizeof(buffer)-1,
            0
                );
        if(receivedBytes > 0){
            buffer[receivedBytes] = '\0';
            //按实际收到的字节数追加，不能依赖字符串结束符
            pendingBuffer.append(
                buffer,
                static_cast<std::size_t>(receivedBytes)
                    );
            //检查pendingBuffer是否超过缓存限制
            if(pendingBuffer.size() > maxPendingBufferSize){
                    logger.warning("客户端消息缓存超过限制,sessionId=" +
                            std::to_string(sessionId));
                const char reply[] = "FRAME_TOO_LARGE\n";
                if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                    logger.error(
                            makeSystemErrorMessage("send")+
                            ",sessionId="+
                            std::to_string(sessionId));
                }
                break;
            }
            std::size_t newlinePosition;
            while((newlinePosition 
                        = pendingBuffer.find('\n'))
                    != std::string::npos){
                std::string completeMessage=
                    pendingBuffer.substr(0,newlinePosition);

                pendingBuffer.erase(0,newlinePosition + 1);//删除已经处理的消息和换行符
                protocol::DeviceData data;//接收完整的数据存入临时data
                //检查是否解析设备数据成功,协议格式错误
                if(!protocol::parseDeviceData(completeMessage,data)){
                    logger.warning(
                            "设备数据格式有误,sessionId=" +
                            std::to_string(sessionId) +
                            ",data:" +
                             completeMessage);
                    const char reply[] = "ERROR\n";
                    if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                        logger.error(
                                makeSystemErrorMessage("send")+
                                ",sessionId="+
                                std::to_string(sessionId));
                        connectionActive = false;
                        break;
                    }
                    continue;
                }
                //解析成功后
                //第一次收到数据时绑定设备ID,一台客户端只能代表一台设备
                if(connectDeviceId.empty()){
                    connectDeviceId = data.deviceId;
                }
                else if(connectDeviceId != data.deviceId){
                        logger.warning(
                                "同一连接尝试更换设备ID，sessionId=" +
                                std::to_string(sessionId) +
                                ",originalDeviceId=" +
                                connectDeviceId +
                                ",newDeviceId=" +
                                data.deviceId);
                    const char reply[] = "ID_ERROR\n";
                    if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                        logger.error(
                                makeSystemErrorMessage("send")+
                                ",sessionId="+
                                std::to_string(sessionId));
                        connectionActive = false;
                        break;
                    }
                    continue;
                }
                //DeviceRegistry内部负责加锁和更新设备信息
                //更新会话
                const gateway::UpdateResult updateResult=
                    registry.update(data,sessionId);
                if(updateResult == gateway::UpdateResult::ReplacedByNewerSession){
                    //拒绝已经失效的旧连接
                    logger.warning(
                            "旧会话退出，无法修改设备状态，sessionId=" +
                            std::to_string(sessionId));
                    const char reply[] = "SESSION_REPLACED\n";
                    if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                        logger.error(
                                makeSystemErrorMessage("send")+
                                ",sessionId="+
                                std::to_string(sessionId));
                    }
                    connectionActive = false;
                    break;
                }
                
                //把有效设备数据保存到SQLite
                if(!telemetryStorage.saveTelemetry(
                            data,
                            sessionId)){
                    logger.error(
                        "设备数据保存失败,deviceId=" +
                        data.deviceId +
                        ",sessionId=" +
                        std::to_string(sessionId));
                    const char reply[] ="STORAGE_ERROR\n";

                    if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                        logger.error(
                                makeSystemErrorMessage("send")+
                                ",sessionId="+
                                std::to_string(sessionId));
                        connectionActive = false;
                        break;
                    }
                    //保存失败继续等待下一条数据
                    continue;
                }

                const char reply[] = "OK\n";
                if(!net::sendAll(clientFd,reply,sizeof(reply)-1)){
                    logger.error(
                            makeSystemErrorMessage("send")+
                            ",sessionId="+
                            std::to_string(sessionId));
                    connectionActive = false;
                    break;
                }
            }
        }
        else if(receivedBytes == 0){
            logger.info(
                    "客户端主动断开,sessionId=" +
                    std::to_string(sessionId));
            break;
        }
        else if(errno == EINTR){
            //recv被信号临时中断
            continue;
        }
        else if(errno == EAGAIN || errno == EWOULDBLOCK){
            continue;
        }
        else{
            logger.error(
                    makeSystemErrorMessage("recv")+
                    ",sessionId="+
                    std::to_string(sessionId));
            break;
        }
    }
    //客户端断开后离线标记
    if(!connectDeviceId.empty()){
        const bool markedOffline =registry.markOfflineIfOwner(
                connectDeviceId,
                sessionId);
        if(!markedOffline){
                logger.warning("旧会话退出，不可修改设备状态,sessionId=" +
                std::to_string(sessionId));

        }
    }
    //客户端处理线程结束日志
    logger.info(
            "客户端处理线程结束，sessionId=" +
            std::to_string(sessionId));
    close(clientFd);
}
//主函数=================================================================================================
int main()
{
    //读取文件配置,加载配置==============================================
    config::GatewayConfig gatewayConfig;

    if(!config::loadGatewayConfig(
        "config/gateway.conf",
        gatewayConfig)){
        std::cerr <<"网关配置加载失败，服务器退出\n";
        return 1;
    }
    //初始化日志===========================================================
    logging::Logger logger(gatewayConfig.logPath);

    if(!logger.open()){
        std::cerr
            <<"日志系统初始化失败，服务器退出\n";
        return 1;
    }
    logger.info("日志系统初始化成功");

    //注册Ctrl+C信号
    struct sigaction signalAction{};

    signalAction.sa_handler = handleStopSignal;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;

    if(sigaction(
                SIGINT,
                &signalAction,
                nullptr) == -1){
        logger.error(
                makeSystemErrorMessage("sigaction"));

        return 1;
    }

    //1.创建监听Socket
    int listenFd = socket(AF_INET,SOCK_STREAM,0);

    if(listenFd == -1)
    {
        const std::error_code error(errno,std::generic_category());
        logger.error( 
                "sock创建失败,errno=" +
                std::to_string(error.value()) +
                ",message=" +
                error.message());
        
        return 1;
    }
    
    //防止端口重启时端口暂时被占用
    int reuse = 1;
    if(setsockopt(
            listenFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) == -1){
        logger.error(
                makeSystemErrorMessage("setsockopt(SO_REUSEADDR)"));
        close(listenFd);
        return 1;
    }
    
    //2.设置服务器地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(gatewayConfig.port);
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);

    //3.将Socket与端口绑定
    if(bind(
        listenFd,
        reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr))==-1){
        
        const std::error_code error(errno,std::generic_category());
        logger.error( 
                "bind创建失败,errno=" +
                std::to_string(error.value()) +
                ",message=" +
                error.message());
        
        close(listenFd);
        return 1;
    }

    //4.开始监听
    if(listen(listenFd,5)==-1){
        const std::error_code error(errno,std::generic_category());
        logger.error( 
                "listen失败,errno=" +
                std::to_string(error.value()) +
                ",message=" +
                error.message());
        
        close(listenFd);
        return 1;
    }

//======================================================================================
    //服务端启动时初始化数据库
    //创建SQLite存储对象
    storage::SQLiteStorage telemetryStorage(gatewayConfig.databasePath);
    //打开数据库并检查数据表
    if(!telemetryStorage.open() || !telemetryStorage.initialize()){
        logger.error("数据库初始化失败，服务器退出");
        
        close(listenFd);
        return 1;
    }
    logger.info( 
            "服务器启动，端口: "+
            std::to_string(gatewayConfig.port)+
        "，等待客户端连接...");

    //5.接受一个客户端连接
    //创建设备注册表，互斥锁由内部管理
    gateway::DeviceRegistry registry;
    std::uint64_t nextSessionId = 1;
    std::vector<std::thread> clientThreads;
    //创建后台线程监测设备状态
    std::thread monitorThread(
        monitorDeviceStatus,
        std::ref(registry),
        gatewayConfig.timeoutSeconds);
    
    //循环接收客户端连接
    while(!stopRequested){
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientFd = accept(
            listenFd,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &clientAddrLen
                );
        if(clientFd==-1){
            if(errno == EINTR){
                break;
            }
            logger.error(makeSystemErrorMessage("accept"));
            continue;
        }
        //生成会话id
        const std::uint64_t sessionId = nextSessionId++;
        //连接日志
        logger.info( "客户端连接成功，sessionId="
                +std::to_string(sessionId));
        
        //给每个客户端设置1秒接收超时
        timeval receiveTimeout{};
        receiveTimeout.tv_sec =1;
        receiveTimeout.tv_usec =0;
        if(setsockopt(
                    clientFd,
                    SOL_SOCKET,
                    SO_RCVTIMEO,
                    &receiveTimeout,
                    sizeof(receiveTimeout)) == -1){
            logger.error(
                    makeSystemErrorMessage("setsockopt(SO_RCVTIMEO)"));

            close(clientFd);
            continue;
        }

        //操作客户端线程
        clientThreads.emplace_back(
            handleClient,
            clientFd,
            sessionId,
            std::ref(registry),
            std::ref(telemetryStorage),
            std::ref(logger),
            gatewayConfig.maxPendingBufferSize);
    }

    logger.info("收到停止信号，服务器准备退出");
    //7.关闭Socket
    close(listenFd);

    if(monitorThread.joinable()){
        monitorThread.join();
    }

    logger.info("设备监控线程已经结束");

    //等待所有客户端线程
    logger.info("正在等待所有客户端线程结束");
    for(std::thread& clientThread :clientThreads){
        if(clientThread.joinable()){
            clientThread.join();
        }
    }
    logger.info("所有客户端线程已经结束");
    logger.info("服务器退出");

    return 0;
}
