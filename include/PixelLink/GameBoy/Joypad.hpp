#pragma once

#include <cstdint>

namespace PixelLink::GameBoy {

enum class JoypadButton : std::uint8_t {
    Right,
    Left,
    Up,
    Down,
    A,
    B,
    Select,
    Start,
};

class Joypad {
public:
    [[nodiscard]] auto Read() const noexcept -> std::uint8_t;

    [[nodiscard]] auto Write(
        std::uint8_t value
    ) noexcept -> bool;

    [[nodiscard]] auto SetButton(
        JoypadButton button,
        bool pressed
    ) noexcept -> bool;

private:
    // P15/P14 selection bits. 0 means selected.
    std::uint8_t select_ = 0x30;

    // Internal state is active-high: 1 means pressed.
    // D-pad:  bit 0 Right, bit 1 Left, bit 2 Up, bit 3 Down.
    // Action: bit 0 A, bit 1 B, bit 2 Select, bit 3 Start.
    std::uint8_t dpadPressed_ = 0x00;
    std::uint8_t actionPressed_ = 0x00;

    [[nodiscard]] auto SelectedLines() const noexcept
        -> std::uint8_t;
};

} // namespace PixelLink::GameBoy
