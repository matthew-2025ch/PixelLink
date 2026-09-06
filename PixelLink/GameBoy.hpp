#pragma once

#include <filesystem>

#include "Bus.hpp"
#include "CPU.hpp"
#include "Cartridge.hpp"

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

private:
    Cartridge cartridge_;
    Bus bus_;
    CPU cpu_;
};

} // namespace PixelLink::GameBoy