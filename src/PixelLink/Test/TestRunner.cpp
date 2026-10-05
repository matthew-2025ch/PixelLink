#include <PixelLink/Utils/Logger.hpp>
#include <PixelLink/Test/TestRunner.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace PixelLink::Test {
using Utils::GetLogger;
namespace {

using Clock = std::chrono::system_clock;
constexpr std::size_t MAX_RUN_LOGS = 10;

auto MakeRunLogPath(
    const std::filesystem::path& directory,
    const std::string_view programName,
    Clock::time_point startedAt
) -> std::filesystem::path {
    for (;;) {
        const auto seconds = std::chrono::floor<std::chrono::seconds>(startedAt);
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            startedAt - seconds).count();
        const auto clockTime = Clock::to_time_t(seconds);
        std::tm localTime{};
#ifdef _WIN32
        if (localtime_s(&localTime, &clockTime) != 0) {
#else
        if (localtime_r(&clockTime, &localTime) == nullptr) {
#endif
            throw std::runtime_error("Could not format the test run start time");
        }

        // Colons are forbidden in Windows filenames, so use hyphens here.
        std::ostringstream timestamp;
        timestamp << std::put_time(&localTime, "%Y-%m-%d %H-%M-%S")
                  << '-' << std::setfill('0') << std::setw(3) << milliseconds;
        const auto path = directory / std::format(
            "[{}] {}.log", timestamp.str(), programName);
        if (!std::filesystem::exists(path)) {
            return path;
        }
        // Avoid appending to an earlier run that started in the same millisecond.
        startedAt += std::chrono::milliseconds(1);
    }
}

void PruneRunLogs(
    const std::filesystem::path& directory,
    const std::filesystem::path& currentLog
) {
    // Only the test programs' timestamped records belong to this policy.
    // Ignore unrelated files, directories and symlinks in the log directory.
    // Optional 's' keeps older run records within the same retention limit.
    static const std::regex runLogName(
        R"(^\[\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2}-\d{3}\] (GameBoyTest|FrontendTest|DesktopTest)s?\.log$)");
    std::vector<std::filesystem::path> logs;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_symlink() && entry.is_regular_file() &&
            std::regex_match(entry.path().filename().string(), runLogName)) {
            logs.push_back(entry.path());
        }
    }
    std::sort(logs.begin(), logs.end(), [](const auto& left, const auto& right) {
        return left.filename().native() < right.filename().native();
    });

    auto remaining = logs.size();
    for (const auto& path : logs) {
        if (remaining <= MAX_RUN_LOGS) {
            break;
        }
        if (path == currentLog) {
            continue;
        }
        std::error_code error;
        const bool removed = std::filesystem::remove(path, error);
        if (removed || !error) {
            --remaining;
        } else {
            GetLogger().warn("Could not remove old log {}: {}", path.string(), error.message());
        }
    }
}

void InitializeLogging(const std::string_view programName, const Clock::time_point startedAt) {
    // Path selection can fail before file logging is ready.
    Utils::InitializeLogger(programName);
    const auto directory = std::filesystem::path(PIXELLINK_TEST_LOG_DIR);
    const auto logPath = MakeRunLogPath(directory, programName, startedAt);
    Utils::InitializeLogger(programName, logPath);
    PruneRunLogs(directory, logPath);
    GetLogger().info("Logs: {}", logPath.string());
}

} // namespace

auto RunTestProgram(
    const std::string_view programName,
    const std::string_view title,
    const std::function<void()>& runSuites
) -> int {
    const auto startedAt = Clock::now();
    int result = 0;
    try {
        InitializeLogging(programName, startedAt);
        GetLogger().info("{}", title);
        runSuites();
        GetLogger().info("ALL TESTS PASSED");
    } catch (const std::exception& error) {
        GetLogger().error("TESTS FAILED: {}", error.what());
        result = 1;
    } catch (...) {
        GetLogger().critical("TESTS FAILED: Unknown exception");
        result = 1;
    }

    Utils::ShutdownLogger();
    return result;
}

} // namespace PixelLink::Test
