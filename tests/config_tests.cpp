#include "config/gateway_config.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <unistd.h>

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

bool writeConfig(
    const std::filesystem::path& path,
    const std::string& content)
{
    std::ofstream output(path);

    if (!output.is_open()) {
        return false;
    }

    output << content;
    return output.good();
}

} // namespace

int main()
{
    const std::filesystem::path tempDirectory =
        std::filesystem::temp_directory_path();

    const std::string uniquePart =
        std::to_string(
            static_cast<long long>(getpid()));

    const std::filesystem::path validPath =
        tempDirectory /
        ("gateway_valid_" + uniquePart + ".conf");

    const std::filesystem::path invalidPath =
        tempDirectory /
        ("gateway_invalid_" + uniquePart + ".conf");

    const std::filesystem::path missingPath =
        tempDirectory /
        ("gateway_missing_" + uniquePart + ".conf");

    expect(
        writeConfig(
            validPath,
            "port=9000\n"
            "timeout_seconds=15\n"
            "database_path=/tmp/test_gateway.db\n"
            "max_pending_buffer_size=8192\n"
            "log_path=/tmp/test_gateway.log\n"),
        "valid temporary config is created");

    config::GatewayConfig validConfig;

    expect(
        config::loadGatewayConfig(
            validPath.string(),
            validConfig),
        "valid config loads");

    expect(
        validConfig.port == 9000,
        "port is loaded");

    expect(
        validConfig.timeoutSeconds == 15,
        "timeout is loaded");

    expect(
        validConfig.databasePath ==
            "/tmp/test_gateway.db",
        "database path is loaded");

    expect(
        validConfig.maxPendingBufferSize == 8192,
        "buffer limit is loaded");

    expect(
        validConfig.logPath ==
            "/tmp/test_gateway.log",
        "log path is loaded");

    expect(
        writeConfig(
            invalidPath,
            "port=9001\n"
            "timeout_seconds=0\n"),
        "invalid temporary config is created");

    config::GatewayConfig unchangedConfig;

    const config::GatewayConfig originalConfig =
        unchangedConfig;

    expect(
        !config::loadGatewayConfig(
            invalidPath.string(),
            unchangedConfig),
        "invalid config is rejected");

    expect(
        unchangedConfig.port ==
            originalConfig.port &&
        unchangedConfig.timeoutSeconds ==
            originalConfig.timeoutSeconds &&
        unchangedConfig.databasePath ==
            originalConfig.databasePath &&
        unchangedConfig.maxPendingBufferSize ==
            originalConfig.maxPendingBufferSize &&
        unchangedConfig.logPath ==
            originalConfig.logPath,
        "failed load leaves config unchanged");

    std::error_code error;
    std::filesystem::remove(missingPath, error);

    config::GatewayConfig missingConfig;

    expect(
        !config::loadGatewayConfig(
            missingPath.string(),
            missingConfig),
        "missing config file is rejected");

    std::filesystem::remove(validPath, error);
    std::filesystem::remove(invalidPath, error);

    if (failureCount != 0) {
        std::cerr
            << failureCount
            << " config test(s) failed\n";

        return 1;
    }

    std::cout
        << "All config tests passed\n";

    return 0;
}
