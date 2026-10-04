#pragma once

#include <functional>
#include <string_view>

#include <spdlog/spdlog.h>

namespace PixelLink::Test {

// Shared by all test suites; each executable owns one synchronous logger.
auto GetLogger() -> spdlog::logger&;

// Initialize logging, run the suites and report a final process result.
auto RunTestProgram(
    std::string_view programName,
    std::string_view title,
    const std::function<void()>& runSuites
) -> int;

} // namespace PixelLink::Test
