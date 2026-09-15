#include <PixelLink/GameBoy/Joypad.hpp>

namespace PixelLink::GameBoy {

auto Joypad::SelectedLines() const noexcept
    -> std::uint8_t {
    std::uint8_t lines = 0x0F;

    // P14 low selects the directional buttons.
    if ((select_ & 0x10) == 0) {
        lines &= static_cast<std::uint8_t>(
            ~dpadPressed_
        );
    }

    // P15 low selects A/B/Select/Start.
    if ((select_ & 0x20) == 0) {
        lines &= static_cast<std::uint8_t>(
            ~actionPressed_
        );
    }

    return lines & 0x0F;
}

auto Joypad::Read() const noexcept -> std::uint8_t {
    // Bits 7-6 read high. Bits 5-4 are selection bits.
    // Bits 3-0 are active-low button input lines.
    return static_cast<std::uint8_t>(
        0xC0 | select_ | SelectedLines()
    );
}

auto Joypad::Write(
    const std::uint8_t value
) noexcept -> bool {
    const std::uint8_t oldLines = SelectedLines();

    // Only P15/P14 are writable.
    select_ = value & 0x30;

    const std::uint8_t newLines = SelectedLines();

    // Joypad interrupt: selected P10-P13 line high -> low.
    return (
        oldLines &
        static_cast<std::uint8_t>(~newLines) &
        0x0F
    ) != 0;
}

auto Joypad::SetButton(
    const JoypadButton button,
    const bool pressed
) noexcept -> bool {
    std::uint8_t* state = nullptr;
    std::uint8_t mask = 0;

    switch (button) {
    case JoypadButton::Right:
        state = &dpadPressed_;
        mask = 0x01;
        break;

    case JoypadButton::Left:
        state = &dpadPressed_;
        mask = 0x02;
        break;

    case JoypadButton::Up:
        state = &dpadPressed_;
        mask = 0x04;
        break;

    case JoypadButton::Down:
        state = &dpadPressed_;
        mask = 0x08;
        break;

    case JoypadButton::A:
        state = &actionPressed_;
        mask = 0x01;
        break;

    case JoypadButton::B:
        state = &actionPressed_;
        mask = 0x02;
        break;

    case JoypadButton::Select:
        state = &actionPressed_;
        mask = 0x04;
        break;

    case JoypadButton::Start:
        state = &actionPressed_;
        mask = 0x08;
        break;
    }

    const std::uint8_t oldLines = SelectedLines();
    const std::uint8_t oldState = *state;

    if (pressed) {
        *state |= mask;
    } else {
        *state &= static_cast<std::uint8_t>(~mask);
    }

    // Ignore repeated key-down/key-up events that do not change state.
    if (*state == oldState) {
        return false;
    }

    const std::uint8_t newLines = SelectedLines();

    return (
        oldLines &
        static_cast<std::uint8_t>(~newLines) &
        0x0F
    ) != 0;
}

} // namespace PixelLink::GameBoy
