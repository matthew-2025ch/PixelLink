#include <PixelLink/GameBoy/PPU.hpp>

namespace PixelLink::GameBoy {

PPU::PPU(Bus& bus)
    : bus_(bus) {
    Reset();
}

void PPU::Reset() {
    lineDot_ = 0;
    SetLY(0);

    if (IsLCDEnabled()) {
        SetMode(Mode::OAMScan);
    } else {
        SetMode(Mode::HBlank);
    }

    UpdateSTAT();
}

void PPU::Step(const std::uint32_t tCycles) {
    // Part 1 intentionally advances one dot at a time. This keeps the
    // state transitions easy to verify before rendering is implemented.
    for (std::uint32_t i = 0; i < tCycles; ++i) {
        StepOneDot();
    }
}

PPU::Mode PPU::GetMode() const noexcept {
    return mode_;
}

std::uint8_t PPU::GetLY() const noexcept {
    return ly_;
}

std::uint16_t PPU::GetLineDot() const noexcept {
    return lineDot_;
}

bool PPU::IsLCDEnabled() const {
    return (bus_.Read(LCDC) & 0x80u) != 0;
}

void PPU::StepOneDot() {
    if (!IsLCDEnabled()) {
        // While the LCD is disabled, LY is held at 0 and the PPU reports
        // mode 0. Rendering timing resumes from the beginning when enabled.
        lineDot_ = 0;
        SetLY(0);
        SetMode(Mode::HBlank);
        UpdateSTAT();
        return;
    }

    // If LCD was just enabled, start a fresh visible scanline.
    if (mode_ == Mode::HBlank && ly_ == 0 && lineDot_ == 0) {
        SetMode(Mode::OAMScan);
    }

    ++lineDot_;

    if (ly_ < VISIBLE_LINES) {
        if (lineDot_ == OAM_SCAN_END) {
            SetMode(Mode::Drawing);
        } else if (lineDot_ == DRAWING_END) {
            // Part 1 uses a fixed 172-dot drawing period.
            SetMode(Mode::HBlank);
        }
    }

    if (lineDot_ >= DOTS_PER_LINE) {
        lineDot_ = 0;
        const std::uint8_t nextLY = static_cast<std::uint8_t>(ly_ + 1u);

        if (nextLY == VISIBLE_LINES) {
            SetLY(nextLY);
            SetMode(Mode::VBlank);
            RequestVBlankInterrupt();
        } else if (nextLY >= TOTAL_LINES) {
            SetLY(0);
            SetMode(Mode::OAMScan);
        } else {
            SetLY(nextLY);

            if (ly_ < VISIBLE_LINES) {
                SetMode(Mode::OAMScan);
            } else {
                SetMode(Mode::VBlank);
            }
        }
    }

    UpdateSTAT();
}

void PPU::SetMode(const Mode mode) {
    mode_ = mode;
    UpdateSTAT();
}

void PPU::SetLY(const std::uint8_t value) {
    ly_ = value;
    bus_.Write(LY, ly_);
    UpdateSTAT();
}

void PPU::UpdateSTAT() {
    std::uint8_t stat = bus_.Read(STAT);

    // Bits 0-1 contain the current PPU mode.
    stat = static_cast<std::uint8_t>((stat & 0xFCu) |
        static_cast<std::uint8_t>(mode_));

    // Bit 2 is the LY == LYC coincidence flag.
    if (ly_ == bus_.Read(LYC)) {
        stat = static_cast<std::uint8_t>(stat | 0x04u);
    } else {
        stat = static_cast<std::uint8_t>(stat & ~0x04u);
    }

    bus_.Write(STAT, stat);
}

void PPU::RequestVBlankInterrupt() {
    const std::uint8_t interruptFlags = bus_.Read(IF);
    bus_.Write(IF, static_cast<std::uint8_t>(interruptFlags | 0x01u));
}

} // namespace PixelLink::GameBoy
