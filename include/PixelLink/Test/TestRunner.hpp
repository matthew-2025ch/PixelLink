#pragma once

#include <functional>
#include <string_view>

namespace PixelLink::Test {

// Own the test run's logging, retention policy and process result.
auto RunTestProgram(
    std::string_view programName,
    std::string_view title,
    const std::function<void()>& runSuites
) -> int;

} // namespace PixelLink::Test
