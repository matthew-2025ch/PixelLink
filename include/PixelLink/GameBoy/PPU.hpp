#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PixelLink::GameBoy {

class Bus;

class PPU {
public:
    enum class Mode : std::uint8_t {
        HBlank = 0,
        VBlank = 1,
        OAMScan = 2,
        Drawing = 3,
    };

    static constexpr std::size_t SCREEN_WIDTH = 160;
    static constexpr std::size_t SCREEN_HEIGHT = 144;
    static constexpr std::size_t FRAMEBUFFER_SIZE =
        SCREEN_WIDTH * SCREEN_HEIGHT;

    using Framebuffer = std::array<std::uint8_t, FRAMEBUFFER_SIZE>;

    explicit PPU(Bus& bus);
    ~PPU();

    void Reset();
    void Step(std::uint32_t tCycles);

    [[nodiscard]] Mode GetMode() const noexcept;
    [[nodiscard]] std::uint8_t GetLY() const noexcept;
    [[nodiscard]] std::uint16_t GetLineDot() const noexcept;
    [[nodiscard]] std::uint8_t GetWindowLine() const noexcept;
    [[nodiscard]] const Framebuffer& GetFramebuffer() const noexcept;

    // CPU-side PPU memory access rules.
    [[nodiscard]] bool CanCPUAccessVRAM() const noexcept;
    [[nodiscard]] bool CanCPUAccessOAM() const noexcept;

private:
    struct Sprite {
        std::uint8_t oamIndex = 0;
        std::uint8_t y = 0;
        std::uint8_t x = 0;
        std::uint8_t tileId = 0;
        std::uint8_t attributes = 0;
    };

    static constexpr std::uint16_t IF   = 0xFF0F;

    static constexpr std::uint16_t LCDC = 0xFF40;
    static constexpr std::uint16_t STAT = 0xFF41;
    static constexpr std::uint16_t SCY  = 0xFF42;
    static constexpr std::uint16_t SCX  = 0xFF43;
    static constexpr std::uint16_t LY   = 0xFF44;
    static constexpr std::uint16_t LYC  = 0xFF45;
    static constexpr std::uint16_t BGP  = 0xFF47;
    static constexpr std::uint16_t OBP0 = 0xFF48;
    static constexpr std::uint16_t OBP1 = 0xFF49;
    static constexpr std::uint16_t WY   = 0xFF4A;
    static constexpr std::uint16_t WX   = 0xFF4B;

    static constexpr std::uint16_t TILE_DATA_UNSIGNED_BASE = 0x8000;
    static constexpr std::uint16_t TILE_DATA_SIGNED_BASE   = 0x9000;
    static constexpr std::uint16_t BG_TILE_MAP_0_BASE      = 0x9800;
    static constexpr std::uint16_t BG_TILE_MAP_1_BASE      = 0x9C00;
    static constexpr std::uint16_t OAM_BASE                = 0xFE00;

    static constexpr std::uint16_t OAM_SCAN_END  = 80;
    static constexpr std::uint16_t DRAWING_END   = 252;
    static constexpr std::uint16_t DOTS_PER_LINE = 456;

    static constexpr std::uint8_t VISIBLE_LINES = 144;
    static constexpr std::uint8_t TOTAL_LINES   = 154;

    static constexpr std::size_t OAM_SPRITE_COUNT = 40;
    static constexpr std::size_t MAX_SPRITES_PER_LINE = 10;

    Bus& bus_;

    Mode mode_ = Mode::OAMScan;
    std::uint8_t ly_ = 0;
    std::uint16_t lineDot_ = 0;
    std::uint8_t windowLine_ = 0;

    // Previous state of the combined STAT interrupt line.
    // STAT requests an interrupt only on a low-to-high transition.
    bool statInterruptLine_ = false;

    Framebuffer framebuffer_{};

    // Raw BG/Window color IDs for the current scanline.
    // Sprite priority depends on the color ID, not the final palette shade.
    std::array<std::uint8_t, SCREEN_WIDTH> bgColorIds_{};

    [[nodiscard]] bool IsLCDEnabled() const;

    [[nodiscard]] std::uint8_t ReadBus(
        std::uint16_t address
    ) const;

    void WriteBus(
        std::uint16_t address,
        std::uint8_t value
    );

    void StepOneDot();
    void SetMode(Mode mode);
    void SetLY(std::uint8_t value);
    void UpdateSTAT();
    void UpdateSTATInterruptLine();
    void RequestVBlankInterrupt();
    void RequestSTATInterrupt();

    void RenderScanline();
    void RenderBackgroundScanline();
    [[nodiscard]] bool RenderWindowScanline();
    void RenderSpriteScanline();

    [[nodiscard]] std::uint16_t GetBackgroundTileMapBase(
        std::uint8_t lcdc
    ) const noexcept;

    [[nodiscard]] std::uint16_t GetWindowTileMapBase(
        std::uint8_t lcdc
    ) const noexcept;

    [[nodiscard]] std::uint16_t GetTileDataAddress(
        std::uint8_t tileId,
        std::uint8_t lcdc
    ) const noexcept;

    [[nodiscard]] std::uint8_t DecodeTilePixel(
        std::uint16_t tileAddress,
        std::uint8_t row,
        std::uint8_t column
    ) const;

    [[nodiscard]] static std::uint8_t ApplyPalette(
        std::uint8_t colorId,
        std::uint8_t palette
    ) noexcept;
};

} // namespace PixelLink::GameBoy
