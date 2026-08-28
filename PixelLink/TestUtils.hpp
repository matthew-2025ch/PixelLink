#pragma once

#include <cstdint>
#include <initializer_list>

#include "Bus.hpp"

namespace Test {

inline void load(
    Bus& bus,
    uint16_t address,
    std::initializer_list<uint8_t> bytes
) {
    for (const uint8_t byte : bytes) {
        bus.write(address++, byte);
    }
}

} // namespace Test
