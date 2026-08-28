#pragma once

#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace Test {

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
    try {
        test();

        std::cout
            << "[PASS] "
            << name
            << '\n';
    }
    catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << name
            << '\n'
            << "       "
            << e.what()
            << '\n';

        throw;
    }
}

} // namespace Test
