#pragma once

#include <filesystem>

#include <PixelLink/GameBoy/APU.hpp>
#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/CPU.hpp>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/GameBoy/Timer.hpp>
#include <PixelLink/GameBoy/Serial.hpp>

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
    [[nodiscard]] auto GetSerial() noexcept -> Serial&;

    [[nodiscard]] auto GetJoypad() noexcept -> Joypad&;
    [[nodiscard]] auto GetJoypad() const noexcept -> const Joypad&;

    [[nodiscard]] auto GetAPU() noexcept -> APU&;
    [[nodiscard]] auto GetAPU() const noexcept -> const APU&;

    GameBoy(const GameBoy&) = delete;
    GameBoy& operator=(const GameBoy&) = delete;

    GameBoy(GameBoy&&) = delete;
    GameBoy& operator=(GameBoy&&) = delete;

private:
    static constexpr std::uint8_t TIMER_INTERRUPT = 1u << 2;
    static constexpr std::uint8_t JOYPAD_INTERRUPT = 1u << 4;

    Cartridge cartridge_;
    Timer timer_;
    // Clock phase at PC=0100 after the DMG ABC boot sequence. DIV writes
    // subsequently reset the timer counter, but do not reset this link clock.
    Serial serial_{0xABCC};
    Joypad joypad_;
    APU apu_;
    Bus bus_;
    PPU ppu_;
    CPU cpu_;

    auto TickDevices(std::uint32_t cycles) -> void;
};

} // namespace PixelLink::GameBoy
