#pragma once

#include <filesystem>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/CPU.hpp>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/GameBoy/Timer.hpp>

namespace PixelLink::GameBoy {

class GameBoy {
public:
    GameBoy();

    auto LoadROM(const std::filesystem::path& path) -> void;
    auto Step() -> int;

    auto SetButton(
        JoypadButton button,
        bool pressed
    ) -> void;

    [[nodiscard]] auto GetCPU() noexcept -> CPU&;
    [[nodiscard]] auto GetCPU() const noexcept -> const CPU&;

    [[nodiscard]] auto GetBus() noexcept -> Bus&;
    [[nodiscard]] auto GetBus() const noexcept -> const Bus&;

    [[nodiscard]] auto GetCartridge() noexcept -> Cartridge&;
    [[nodiscard]] auto GetCartridge() const noexcept
        -> const Cartridge&;

    [[nodiscard]] auto GetPPU() noexcept -> PPU&;
    [[nodiscard]] auto GetPPU() const noexcept -> const PPU&;

    [[nodiscard]] auto GetTimer() noexcept -> Timer&;
    [[nodiscard]] auto GetTimer() const noexcept -> const Timer&;

    [[nodiscard]] auto GetJoypad() noexcept -> Joypad&;
    [[nodiscard]] auto GetJoypad() const noexcept -> const Joypad&;

    GameBoy(const GameBoy&) = delete;
    GameBoy& operator=(const GameBoy&) = delete;

    GameBoy(GameBoy&&) = delete;
    GameBoy& operator=(GameBoy&&) = delete;

private:
    static constexpr std::uint8_t TIMER_INTERRUPT = 1u << 2;
    static constexpr std::uint8_t JOYPAD_INTERRUPT = 1u << 4;

    Cartridge cartridge_;
    Timer timer_;
    Joypad joypad_;
    Bus bus_;
    PPU ppu_;
    CPU cpu_;
};

} // namespace PixelLink::GameBoy
