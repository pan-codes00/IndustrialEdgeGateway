#include "server/gateway_server.h"

#include "common/socket_utils.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <string>
#include <system_error>
#include <utility>

namespace {

std::string makeSystemErrorMessage(const std::string& operation)
{
    // 立即保存errno，避免后续函数修改它。
    const int errorNumber = errno;
    const std::error_code error(
        errorNumber,
        std::generic_category());

    return operation +
        "失败，errno=" +
        std::to_string(errorNumber) +
        ",message=" +
        error.message();
}

void handleStopSignal(int)
{
    // 信号处理函数只修改标志，不执行日志或资源释放。
    server::GatewayServer::requestStop();
}

} // namespace

namespace server {

volatile std::sig_atomic_t GatewayServer::stopRequested_ = 0;

GatewayServer::GatewayServer(config::GatewayConfig config)
    : config_(std::move(config)),
      logger_(config_.logPath),
      telemetryStorage_(config_.databasePath),
      messageProcessor_(registry_, telemetryStorage_, logger_)
{
}

GatewayServer::~GatewayServer()
{
    requestStop();
    closeListeningSocket();

    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    waitForClientWorkers();
}

void GatewayServer::requestStop() noexcept
{
    stopRequested_ = 1;
}

bool GatewayServer::isStopRequested() noexcept
{
    return stopRequested_ != 0;
}

bool GatewayServer::initialize()
{
    stopRequested_ = 0;

    if (!logger_.open()) {
        return false;
    }
    logger_.info("日志系统初始化成功");

    if (!installSignalHandler()) {
        return false;
    }

    if (!createListeningSocket()) {
        return false;
    }

    if (!telemetryStorage_.open() ||
        !telemetryStorage_.initialize()) {
        logger_.error("数据库初始化失败，服务器退出");
        closeListeningSocket();
        return false;
    }

    initialized_ = true;
    logger_.info(
        "服务器启动，端口: " +
        std::to_string(config_.port) +
        "，等待客户端连接...");

    return true;
}

int GatewayServer::run()
{
    if (!initialized_) {
        return 1;
    }

    monitorThread_ = std::thread(
        &GatewayServer::monitorDeviceStatus,
        this);

    acceptConnections();

    logger_.info("收到停止信号，服务器准备退出");
    closeListeningSocket();

    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    logger_.info("设备监控线程已经结束");

    logger_.info("正在等待所有客户端线程结束");
    waitForClientWorkers();
    logger_.info("所有客户端线程已经结束");
    logger_.info("服务器退出");

    return 0;
}

bool GatewayServer::installSignalHandler()
{
    struct sigaction signalAction {};
    signalAction.sa_handler = handleStopSignal;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;

    if (sigaction(SIGINT, &signalAction, nullptr) == -1) {
        logger_.error(makeSystemErrorMessage("sigaction"));
        return false;
    }

    return true;
}

bool GatewayServer::createListeningSocket()
{
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ == -1) {
        logger_.error(makeSystemErrorMessage("socket"));
        return false;
    }

    int reuse = 1;
    if (setsockopt(
            listenFd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) == -1) {
        logger_.error(
            makeSystemErrorMessage(
                "setsockopt(SO_REUSEADDR)"));
        closeListeningSocket();
        return false;
    }

    sockaddr_in serverAddress {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config_.port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            listenFd_,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1) {
        logger_.error(makeSystemErrorMessage("bind"));
        closeListeningSocket();
        return false;
    }

    if (listen(listenFd_, 5) == -1) {
        logger_.error(makeSystemErrorMessage("listen"));
        closeListeningSocket();
        return false;
    }

    timeval acceptTimeout {};
    acceptTimeout.tv_sec = 1;
    acceptTimeout.tv_usec = 0;

    if (setsockopt(
            listenFd_,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &acceptTimeout,
            sizeof(acceptTimeout)) == -1) {
        logger_.error(
            makeSystemErrorMessage(
                "setsockopt(SO_RCVTIMEO)"));
        closeListeningSocket();
        return false;
    }

    return true;
}

void GatewayServer::closeListeningSocket() noexcept
{
    if (listenFd_ != -1) {
        close(listenFd_);
        listenFd_ = -1;
    }
}

void GatewayServer::acceptConnections()
{
    while (!isStopRequested()) {
        removeFinishedClientWorkers();

        sockaddr_in clientAddress {};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const int clientFd = accept(
            listenFd_,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength);

        if (clientFd == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }

            logger_.error(makeSystemErrorMessage("accept"));
            continue;
        }

        if (isStopRequested()) {
            close(clientFd);
            break;
        }

        const std::uint64_t sessionId = nextSessionId_++;
        logger_.info(
            "客户端连接成功，sessionId=" +
            std::to_string(sessionId));

        timeval receiveTimeout {};
        receiveTimeout.tv_sec = 1;
        receiveTimeout.tv_usec = 0;

        if (setsockopt(
                clientFd,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &receiveTimeout,
                sizeof(receiveTimeout)) == -1) {
            logger_.error(
                makeSystemErrorMessage(
                    "setsockopt(SO_RCVTIMEO)"));
            close(clientFd);
            continue;
        }

        startClientWorker(clientFd, sessionId);
    }
}

void GatewayServer::startClientWorker(
    int clientFd,
    std::uint64_t sessionId)
{
    auto finished =
        std::make_shared<std::atomic_bool>(false);

    clientWorkers_.push_back(ClientWorker {
        std::thread(
            [this, clientFd, sessionId, finished]() {
                handleClient(clientFd, sessionId);
                finished->store(true);
            }),
        std::move(finished)
    });
}

void GatewayServer::handleClient(
    int clientFd,
    std::uint64_t sessionId)
{
    std::string pendingBuffer;
    std::string connectedDeviceId;
    bool connectionActive = true;

    while (connectionActive && !isStopRequested()) {
        char buffer[1024] {};
        const ssize_t receivedBytes = recv(
            clientFd,
            buffer,
            sizeof(buffer) - 1,
            0);

        if (receivedBytes > 0) {
            pendingBuffer.append(
                buffer,
                static_cast<std::size_t>(receivedBytes));

            if (pendingBuffer.size() >
                config_.maxPendingBufferSize) {
                logger_.warning(
                    "客户端消息缓存超过限制,sessionId=" +
                    std::to_string(sessionId));

                sendReply(
                    clientFd,
                    "FRAME_TOO_LARGE\n",
                    sessionId);
                break;
            }

            std::size_t newlinePosition = 0;
            while ((newlinePosition = pendingBuffer.find('\n')) !=
                   std::string::npos) {
                const std::string completeMessage =
                    pendingBuffer.substr(0, newlinePosition);

                pendingBuffer.erase(0, newlinePosition + 1);

                const MessageProcessResult result =
                    messageProcessor_.processCompleteMessage(
                        completeMessage,
                        connectedDeviceId,
                        sessionId);

                if (!sendReply(
                        clientFd,
                        result.reply,
                        sessionId)) {
                    connectionActive = false;
                    break;
                }

                if (result.action ==
                    MessageAction::CloseConnection) {
                    connectionActive = false;
                    break;
                }
            }
        } else if (receivedBytes == 0) {
            logger_.info(
                "客户端主动断开,sessionId=" +
                std::to_string(sessionId));
            break;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        } else {
            logger_.error(
                makeSystemErrorMessage("recv") +
                ",sessionId=" +
                std::to_string(sessionId));
            break;
        }
    }

    if (!connectedDeviceId.empty()) {
        const bool markedOffline =
            registry_.markOfflineIfOwner(
                connectedDeviceId,
                sessionId);

        if (!markedOffline) {
            logger_.warning(
                "旧会话退出，不可修改设备状态,sessionId=" +
                std::to_string(sessionId));
        }
    }

    logger_.info(
        "客户端处理线程结束，sessionId=" +
        std::to_string(sessionId));
    close(clientFd);
}

bool GatewayServer::sendReply(
    int clientFd,
    std::string_view reply,
    std::uint64_t sessionId)
{
    if (net::sendAll(clientFd, reply.data(), reply.size())) {
        return true;
    }

    logger_.error(
        makeSystemErrorMessage("send") +
        ",sessionId=" +
        std::to_string(sessionId));
    return false;
}

void GatewayServer::monitorDeviceStatus()
{
    while (!isStopRequested()) {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));

        if (isStopRequested()) {
            break;
        }

        registry_.markTimedOut(
            std::chrono::seconds(config_.timeoutSeconds));
    }
}

void GatewayServer::removeFinishedClientWorkers()
{
    auto worker = clientWorkers_.begin();

    while (worker != clientWorkers_.end()) {
        if (!worker->finished->load()) {
            ++worker;
            continue;
        }

        if (worker->thread.joinable()) {
            worker->thread.join();
        }
        worker = clientWorkers_.erase(worker);
    }
}

void GatewayServer::waitForClientWorkers()
{
    for (ClientWorker& worker : clientWorkers_) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }
    clientWorkers_.clear();
}

} // namespace server
