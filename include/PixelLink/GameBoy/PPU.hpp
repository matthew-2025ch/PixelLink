#pragma once

#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>

namespace PixelLink::GameBoy {

class PPU {
public:
    enum class Mode : std::uint8_t {
        HBlank = 0,
        VBlank = 1,
        OAMScan = 2,
        Drawing = 3,
    };

    explicit PPU(Bus& bus);

    void Reset();
    void Step(std::uint32_t tCycles);

    [[nodiscard]] Mode GetMode() const noexcept;
    [[nodiscard]] std::uint8_t GetLY() const noexcept;
    [[nodiscard]] std::uint16_t GetLineDot() const noexcept;

private:
    static constexpr std::uint16_t LCDC = 0xFF40;
    static constexpr std::uint16_t STAT = 0xFF41;
    static constexpr std::uint16_t LY   = 0xFF44;
    static constexpr std::uint16_t LYC  = 0xFF45;
    static constexpr std::uint16_t IF   = 0xFF0F;

    static constexpr std::uint16_t OAM_SCAN_END = 80;
    static constexpr std::uint16_t DRAWING_END  = 252;
    static constexpr std::uint16_t DOTS_PER_LINE = 456;

    static constexpr std::uint8_t VISIBLE_LINES = 144;
    static constexpr std::uint8_t TOTAL_LINES = 154;

    Bus& bus_;
    Mode mode_ = Mode::OAMScan;
    std::uint8_t ly_ = 0;
    std::uint16_t lineDot_ = 0;

    [[nodiscard]] bool IsLCDEnabled() const;

    void StepOneDot();
    void SetMode(Mode mode);
    void SetLY(std::uint8_t value);
    void UpdateSTAT();
    void RequestVBlankInterrupt();
};

} // namespace PixelLink::GameBoy
