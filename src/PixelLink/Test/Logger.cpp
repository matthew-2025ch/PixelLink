#include <PixelLink/Test/Logger.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace PixelLink::Test {
namespace {

using Clock = std::chrono::system_clock;
constexpr std::size_t MAX_RUN_LOGS = 10;
constexpr auto LOG_PATTERN = "[%l %Y-%m-%d %H:%M:%S:%e] %v";

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
    static const std::regex runLogName(
        R"(^\[\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2}-\d{3}\] (GameBoyTests|FrontendTests|DesktopTests)\.log$)");
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
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_pattern(std::string("%^") + LOG_PATTERN + "%$");

    // Install the console first so a file setup failure can still be reported.
    auto logger = std::make_shared<spdlog::logger>(std::string(programName), console);
    spdlog::set_default_logger(logger);

    const auto directory = std::filesystem::path(PIXELLINK_TEST_LOG_DIR);
    const auto logPath = MakeRunLogPath(directory, programName, startedAt);
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string());
    file->set_pattern(LOG_PATTERN);
    logger->sinks().push_back(file);

    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::cfg::load_env_levels();
    PruneRunLogs(directory, logPath);
    logger->info("Logs: {}", logPath.string());
}

} // namespace

auto GetLogger() -> spdlog::logger& {
    return *spdlog::default_logger_raw();
}

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

    GetLogger().flush();
    spdlog::shutdown();
    return result;
}

} // namespace PixelLink::Test
