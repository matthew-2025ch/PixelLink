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
    // Called after the bus stores a CPU write to LCDC, STAT or LYC.
    void OnRegisterWrite(std::uint16_t address, std::uint8_t oldValue);

    [[nodiscard]] Mode GetMode() const noexcept;
    [[nodiscard]] std::uint8_t GetLY() const noexcept;
    [[nodiscard]] std::uint16_t GetLineDot() const noexcept;
    [[nodiscard]] std::uint8_t GetWindowLine() const noexcept;
    [[nodiscard]] const Framebuffer& GetFramebuffer() const noexcept;

    // CPU-side PPU memory access rules.
    [[nodiscard]] bool CanCPUAccessVRAM(bool write = false) const noexcept;
    [[nodiscard]] bool CanCPUAccessOAM(bool write = false) const noexcept;

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

    static constexpr std::uint16_t OAM_SCAN_END  = 84;
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
    std::uint8_t physicalLine_ = 0;
    bool firstLineAfterEnable_ = false;

    // Previous state of the combined STAT interrupt line.
    // STAT requests an interrupt only on a low-to-high transition.
    bool statInterruptLine_ = false;

    Framebuffer framebuffer_{};

    // Fetch bytes on separate two-dot phases; apply palettes only at LCD output.
    enum class FetchPhase { Tile, Low, High, Sleep, Push };
    FetchPhase fetchPhase_ = FetchPhase::Tile;
    std::uint8_t fetchDot_ = 0;
    std::uint8_t fetchTile_ = 0;
    std::uint8_t fetchLow_ = 0;
    std::uint8_t fetchHigh_ = 0;
    std::uint16_t fetchAddress_ = 0;
    std::uint8_t fetchX_ = 0;
    bool discardFirstTile_ = true;
    std::array<std::uint8_t, 16> backgroundFIFO_{};
    std::size_t backgroundHead_ = 0;
    std::size_t backgroundSize_ = 0;

    struct ObjectPixel {
        std::uint8_t color = 0;
        std::uint8_t attributes = 0;
    };
    std::array<ObjectPixel, 8> objectFIFO_{};
    std::size_t objectHead_ = 0;
    std::array<Sprite, MAX_SPRITES_PER_LINE> lineSprites_{};
    std::size_t spriteCount_ = 0;
    std::size_t nextSprite_ = 0;
    Sprite fetchingSprite_{};
    std::uint8_t spriteFetchDot_ = 0;
    std::uint8_t spriteLow_ = 0;
    std::uint8_t spriteHigh_ = 0;
    std::uint16_t spriteAddress_ = 0;
    int spriteWait_ = 0;
    int previousSpriteTile_ = -1;
    bool objectFetchActive_ = false;
    bool firstObjectFetch_ = true;

    std::uint16_t outputX_ = 0;
    std::uint8_t startupDots_ = 0;
    std::uint8_t discardPixels_ = 0;
    std::uint8_t fineScroll_ = 0;
    bool windowYTriggered_ = false;
    bool windowActive_ = false;
    bool windowRendered_ = false;
    int windowLeft_ = 0;

    [[nodiscard]] bool IsLCDEnabled() const;

    [[nodiscard]] std::uint8_t ReadBus(
        std::uint16_t address
    ) const;

    void WriteBus(
        std::uint16_t address,
        std::uint8_t value
    );

    void StepOneDot();
    void ResetPipeline();
    void ScanOAM();
    void BeginDrawing();
    void StepDrawing();
    void ResetFetcher(bool discardFirstTile);
    void StepFetcher();
    [[nodiscard]] bool PushBackgroundPixels();
    [[nodiscard]] bool StartObjectFetch();
    void StepObjectFetcher();
    void MergeObjectPixels();
    void OutputPixel();
    void SetMode(Mode mode);
    void SetLY(std::uint8_t value);
    void UpdateSTAT();
    void UpdateSTATInterruptLine();
    void RequestVBlankInterrupt();
    void RequestSTATInterrupt();


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

    [[nodiscard]] static std::uint8_t ApplyPalette(
        std::uint8_t colorId,
        std::uint8_t palette
    ) noexcept;
};

} // namespace PixelLink::GameBoy
