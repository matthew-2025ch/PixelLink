#pragma once

#include <cstdint>
#include <initializer_list>

#include "Bus.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test {

inline void Load(
    Bus& bus,
    uint16_t address,
    std::initializer_list<uint8_t> bytes
) {
    for (const uint8_t byte : bytes) {
        bus.Write(address++, byte);
    }
}

} // namespace PixelLink::Test
