#pragma once

#include <filesystem>
#include <string_view>

#include <spdlog/spdlog.h>

namespace PixelLink::Utils {

// The process owns one synchronous logger shared by its components.
auto GetLogger() -> spdlog::logger&;

// Always install console logging first; an optional file receives the same
// records. A file setup failure leaves the console logger available.
void InitializeLogger(std::string_view name, const std::filesystem::path& logFile = {});
void ShutdownLogger();

} // namespace PixelLink::Utils
