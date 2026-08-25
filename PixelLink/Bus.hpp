#pragma once

#include <array>
#include <cstdint>

class Bus {
public:
    auto read(uint16_t address) const -> uint8_t;
    auto write(uint16_t address, uint8_t value) -> void;

private:
    std::array<uint8_t, 0x10000> memory{};
};
