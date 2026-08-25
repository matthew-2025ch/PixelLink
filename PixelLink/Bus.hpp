#pragma once

#include <array>
#include <cstdint>
#include "Cartridge.hpp"

class Bus {
public:
    Bus() = default;

    explicit Bus(Cartridge& cartridge);

    auto insertCartridge(Cartridge& cartridge) noexcept
        -> void;

    auto removeCartridge() noexcept
        -> void;

    [[nodiscard]]
    auto read(uint16_t address) const -> uint8_t;

    auto write(
        uint16_t address,
        uint8_t value
    ) -> void;

private:
    Cartridge* cartridge = nullptr;

    std::array<uint8_t, 0x10000> memory{};
};