#include "storage/sqlite_storage.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

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

void removeDatabaseFiles(
    const std::filesystem::path& databasePath)
{
    std::error_code error;

    std::filesystem::remove(
        databasePath,
        error);

    std::filesystem::remove(
        databasePath.string() + "-wal",
        error);

    std::filesystem::remove(
        databasePath.string() + "-shm",
        error);
}

} // namespace

int main()
{
    const std::filesystem::path databasePath =
        std::filesystem::temp_directory_path() /
        (
            "gateway_storage_test_" +
            std::to_string(
                static_cast<long long>(getpid())) +
            ".db"
        );

    //清理上一次异常退出可能留下的临时文件
    removeDatabaseFiles(databasePath);

    {
        storage::SQLiteStorage databaseStorage(
            databasePath.string());

        expect(
            databaseStorage.open(),
            "temporary database opens");

        expect(
            databaseStorage.initialize(),
            "database initializes");

        std::vector<storage::TelemetryRecord> records;

        expect(
            databaseStorage.queryLatestTelemetry(
                "missing",
                10,
                records),
            "missing device query succeeds");

        expect(
            records.empty(),
            "missing device returns no records");

        const protocol::DeviceData firstData{
            "deviceA",
            "temperature",
            25.5
        };

        const protocol::DeviceData secondData{
            "deviceA",
            "temperature",
            26.0
        };

        const protocol::DeviceData otherDeviceData{
            "deviceB",
            "speed",
            1000.0
        };

        expect(
            databaseStorage.saveTelemetry(
                firstData,
                10),
            "first record saves");

        expect(
            databaseStorage.saveTelemetry(
                secondData,
                11),
            "second record saves");

        expect(
            databaseStorage.saveTelemetry(
                otherDeviceData,
                20),
            "other device record saves");

        expect(
            databaseStorage.queryLatestTelemetry(
                "deviceA",
                1,
                records),
            "limited latest query succeeds");

        expect(
            records.size() == 1,
            "limit restricts result count");

        if (records.size() == 1) {
            expect(
                std::abs(
                    records[0].value - 26.0) < 1e-9,
                "latest record is returned");

            expect(
                records[0].sessionId == 11,
                "latest session ID is returned");

            expect(
                records[0].createdAt > 0,
                "creation timestamp is stored");
        }

        expect(
            databaseStorage.queryLatestTelemetry(
                "deviceA",
                10,
                records),
            "full device query succeeds");

        expect(
            records.size() == 2,
            "only requested device records returned");

        if (records.size() == 2) {
            expect(
                records[0].id > records[1].id,
                "records use newest-first order");
        }

        expect(
            !databaseStorage.queryLatestTelemetry(
                "",
                10,
                records),
            "empty device ID is rejected");

        expect(
            records.empty(),
            "failed query clears output records");

        expect(
            !databaseStorage.queryLatestTelemetry(
                "deviceA",
                0,
                records),
            "zero limit is rejected");
    }

    //此时SQLiteStorage已经析构，可以安全删除数据库
    removeDatabaseFiles(databasePath);

    if (failureCount != 0) {
        std::cerr
            << failureCount
            << " SQLite storage test(s) failed\n";

        return 1;
    }

    std::cout
        << "All SQLite storage tests passed\n";

    return 0;
}
