#pragma once

#include <chrono>
#include <format>
#include <stdexcept>
#include <string_view>

#include <PixelLink/Test/Logger.hpp>

namespace PixelLink::Test {

#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            throw std::runtime_error(                                 \
                std::format(                                         \
                    "CHECK failed: {} at {}:{}",                      \
                    #expr,                                            \
                    __FILE__,                                         \
                    __LINE__                                          \
                )                                                     \
            );                                                        \
        }                                                             \
    } while (false)

template <typename Func>
void run(
    std::string_view name,
    Func test
) {
    const auto start = std::chrono::steady_clock::now();
    const auto elapsedMilliseconds = [&] {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    };

    try {
        test();

        GetLogger().info("[PASS] {} ({:.2f} ms)", name, elapsedMilliseconds());
    }
    catch (const std::exception& e) {
        GetLogger().error("[FAIL] {} ({:.2f} ms)\n       {}",
            name, elapsedMilliseconds(), e.what());

        throw;
    }
    catch (...) {
        GetLogger().error("[FAIL] {} ({:.2f} ms)\n       Unknown exception",
            name, elapsedMilliseconds());
        throw;
    }
}

} // namespace PixelLink::Test
