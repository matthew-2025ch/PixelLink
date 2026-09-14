#pragma once

#include <filesystem>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/CPU.hpp>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/PPU.hpp>

namespace PixelLink::GameBoy {

class GameBoy {
public:
    GameBoy();

    auto LoadROM(const std::filesystem::path& path) -> void;
    auto Step() -> int;

    [[nodiscard]] auto GetCPU() noexcept -> CPU&;
    [[nodiscard]] auto GetCPU() const noexcept -> const CPU&;

    [[nodiscard]] auto GetBus() noexcept -> Bus&;
    [[nodiscard]] auto GetBus() const noexcept -> const Bus&;

    [[nodiscard]] auto GetCartridge() noexcept -> Cartridge&;
    [[nodiscard]] auto GetCartridge() const noexcept
        -> const Cartridge&;

    [[nodiscard]] auto GetPPU() noexcept -> PPU&;
    [[nodiscard]] auto GetPPU() const noexcept -> const PPU&;

private:
    Cartridge cartridge_;
    Bus bus_;
    PPU ppu_;
    CPU cpu_;
};

} // namespace PixelLink::GameBoy
