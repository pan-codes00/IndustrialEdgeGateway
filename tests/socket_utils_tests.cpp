#include "common/socket_utils.h"

#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace {

int failureCount = 0;

void expect(
    bool condition,
    const std::string& testName)
{
    if (condition) {
        std::cout
            << "[PASS] "
            << testName
            << '\n';

        return;
    }

    std::cerr
        << "[FAIL] "
        << testName
        << '\n';

    ++failureCount;
}

bool createSocketPair(int sockets[2])
{
    return socketpair(
        AF_UNIX,
        SOCK_STREAM,
        0,
        sockets) == 0;
}

} // namespace

int main()
{
    int sockets[2]{-1, -1};

    if (!createSocketPair(sockets)) {
        std::cerr
            << "Unable to create socket pair\n";

        return 1;
    }

    const std::string messages =
        "OK\nERROR\n";

    expect(
        net::sendAll(
            sockets[0],
            messages.data(),
            messages.size()),
        "sendAll sends complete payload");

    std::string pendingBuffer;
    std::string reply;

    expect(
        net::receiveLine(
            sockets[1],
            pendingBuffer,
            reply),
        "first line is received");

    expect(
        reply == "OK",
        "first line content is correct");

    expect(
        net::receiveLine(
            sockets[1],
            pendingBuffer,
            reply),
        "second line is received");

    expect(
        reply == "ERROR",
        "second line content is correct");

    close(sockets[0]);
    close(sockets[1]);

    int closedPeerSockets[2]{-1, -1};

    if (!createSocketPair(closedPeerSockets)) {
        std::cerr
            << "Unable to create closed-peer pair\n";

        return 1;
    }

    close(closedPeerSockets[1]);

    expect(
        !net::sendAll(
            closedPeerSockets[0],
            "X",
            1),
        "closed peer does not terminate process");

    close(closedPeerSockets[0]);

    int incompleteSockets[2]{-1, -1};

    if (!createSocketPair(incompleteSockets)) {
        std::cerr
            << "Unable to create incomplete pair\n";

        return 1;
    }

    const std::string incompleteMessage =
        "PARTIAL";

    net::sendAll(
        incompleteSockets[0],
        incompleteMessage.data(),
        incompleteMessage.size());

    close(incompleteSockets[0]);

    pendingBuffer.clear();
    reply.clear();

    expect(
        !net::receiveLine(
            incompleteSockets[1],
            pendingBuffer,
            reply),
        "connection close rejects incomplete line");

    expect(
        pendingBuffer == incompleteMessage,
        "incomplete bytes remain in buffer");

    close(incompleteSockets[1]);

    if (failureCount != 0) {
        std::cerr
            << failureCount
            << " socket utility test(s) failed\n";

        return 1;
    }

    std::cout
        << "All socket utility tests passed\n";

    return 0;
}