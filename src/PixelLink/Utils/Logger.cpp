#include <PixelLink/Utils/Logger.hpp>

#include <memory>
#include <string>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace PixelLink::Utils {
namespace {
constexpr auto LOG_PATTERN = "[%l %Y-%m-%d %H:%M:%S:%e] %v";
}

auto GetLogger() -> spdlog::logger& {
    return *spdlog::default_logger_raw();
}

void InitializeLogger(std::string_view name, const std::filesystem::path& logFile) {
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_pattern(std::string("%^") + LOG_PATTERN + "%$");

    auto logger = std::make_shared<spdlog::logger>(std::string(name), console);
    spdlog::set_default_logger(logger);
    if (!logFile.empty()) {
        auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile.string());
        file->set_pattern(LOG_PATTERN);
        logger->sinks().push_back(file);
    }
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::cfg::load_env_levels();
}

void ShutdownLogger() {
    GetLogger().flush();
    spdlog::shutdown();
}

} // namespace PixelLink::Utils
