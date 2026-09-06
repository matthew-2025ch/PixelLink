#pragma once

#include <array>
#include <cstdint>
#include "Cartridge.hpp"
#include "Timer.hpp"

namespace PixelLink::GameBoy {

class Bus {
public:
    Bus() = default;

    explicit Bus(Cartridge& cartridge);

    auto InsertCartridge(Cartridge& cartridge) noexcept -> void;
    auto RemoveCartridge() noexcept -> void;
    [[nodiscard]] auto Read(uint16_t address) const -> uint8_t;
    auto Write(uint16_t address, uint8_t value) -> void;

    auto Tick(uint32_t tCycle) -> void;

private:
    Timer timer_;
    Cartridge* cartridge_ = nullptr;

    // Used only when no cartridge is inserted.
    // This keeps CPU unit tests simple.
    std::array<uint8_t, 0x8000> testRom_{};
    std::array<uint8_t, 0x2000> vram_{};
    std::array<uint8_t, 0x2000> wram_{};
    std::array<uint8_t, 0x00A0> oam_{};
    std::array<uint8_t, 0x0080> io_{};
    std::array<uint8_t, 0x007F> hram_{};
    uint8_t ie_ = 0;
};

} // namespace PixelLink::GameBoy