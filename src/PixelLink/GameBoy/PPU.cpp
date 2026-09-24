#include <PixelLink/GameBoy/PPU.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>

namespace PixelLink::GameBoy {

PPU::PPU(Bus& bus)
    : bus_(bus) {
    bus_.AttachPPU(*this);
    Reset();
}

PPU::~PPU() {
    bus_.DetachPPU(*this);
}

void PPU::Reset() {
    lineDot_ = 0;
    windowLine_ = 0;
    statInterruptLine_ = false;

    framebuffer_.fill(0);
    bgColorIds_.fill(0);

    SetLY(0);

    if (IsLCDEnabled()) {
        SetMode(Mode::OAMScan);
    } else {
        SetMode(Mode::HBlank);
    }

    UpdateSTAT();
}

void PPU::Step(const std::uint32_t tCycles) {
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

std::uint8_t PPU::GetWindowLine() const noexcept {
    return windowLine_;
}

const PPU::Framebuffer& PPU::GetFramebuffer() const noexcept {
    return framebuffer_;
}

bool PPU::CanCPUAccessVRAM() const noexcept {
    if (!IsLCDEnabled()) {
        return true;
    }

    // VRAM is blocked only during Mode 3.
    return mode_ != Mode::Drawing;
}

bool PPU::CanCPUAccessOAM() const noexcept {
    if (!IsLCDEnabled()) {
        return true;
    }

    // OAM is blocked during Modes 2 and 3.
    return mode_ == Mode::HBlank ||
           mode_ == Mode::VBlank;
}

bool PPU::IsLCDEnabled() const {
    return (ReadBus(LCDC) & 0x80u) != 0;
}

std::uint8_t PPU::ReadBus(
    const std::uint16_t address
) const {
    return bus_.Read(address, BusAccess::PPU);
}

void PPU::WriteBus(
    const std::uint16_t address,
    const std::uint8_t value
) {
    bus_.Write(address, value, BusAccess::PPU);
}

void PPU::StepOneDot() {
    if (!IsLCDEnabled()) {
        lineDot_ = 0;
        windowLine_ = 0;

        SetLY(0);
        SetMode(Mode::HBlank);
        UpdateSTAT();
        return;
    }

    // Start a fresh scanline when the LCD has just been enabled.
    if (mode_ == Mode::HBlank && ly_ == 0 && lineDot_ == 0) {
        SetMode(Mode::OAMScan);
    }

    ++lineDot_;

    if (ly_ < VISIBLE_LINES) {
        if (lineDot_ == OAM_SCAN_END) {
            SetMode(Mode::Drawing);
        } else if (lineDot_ == DRAWING_END) {
            // Parts 2-4 still use fixed Mode 3 timing.
            // The whole scanline is rendered when pixel transfer completes.
            RenderScanline();
            SetMode(Mode::HBlank);
        }
    }

    if (lineDot_ >= DOTS_PER_LINE) {
        lineDot_ = 0;

        const std::uint8_t nextLY =
            static_cast<std::uint8_t>(ly_ + 1u);

        if (nextLY == VISIBLE_LINES) {
            SetLY(nextLY);
            SetMode(Mode::VBlank);
            RequestVBlankInterrupt();
        } else if (nextLY >= TOTAL_LINES) {
            SetLY(0);
            windowLine_ = 0;
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
    WriteBus(LY, ly_);
    UpdateSTAT();
}

void PPU::UpdateSTAT() {
    const bool lcdEnabled = IsLCDEnabled();

    // Bits 3-6 are writable interrupt-select bits.
    // Bit 7 reads as 1 on DMG.
    // Bits 0-2 are produced by the PPU.
    std::uint8_t stat = static_cast<std::uint8_t>(
        (ReadBus(STAT) & 0x78u) | 0x80u
    );

    if (lcdEnabled) {
        stat = static_cast<std::uint8_t>(
            stat |
            static_cast<std::uint8_t>(mode_)
        );

        if (ly_ == ReadBus(LYC)) {
            stat = static_cast<std::uint8_t>(
                stat | 0x04u
            );
        }
    }

    WriteBus(STAT, stat);
    UpdateSTATInterruptLine();
}

void PPU::UpdateSTATInterruptLine() {
    bool interruptLine = false;

    if (IsLCDEnabled()) {
        const std::uint8_t stat = ReadBus(STAT);

        const bool lycSource =
            (stat & 0x40u) != 0 &&
            (stat & 0x04u) != 0;

        const bool mode2Source =
            (stat & 0x20u) != 0 &&
            mode_ == Mode::OAMScan;

        const bool mode1Source =
            (stat & 0x10u) != 0 &&
            mode_ == Mode::VBlank;

        const bool mode0Source =
            (stat & 0x08u) != 0 &&
            mode_ == Mode::HBlank;

        interruptLine =
            lycSource ||
            mode2Source ||
            mode1Source ||
            mode0Source;
    }

    if (interruptLine && !statInterruptLine_) {
        RequestSTATInterrupt();
    }

    statInterruptLine_ = interruptLine;
}

void PPU::RequestVBlankInterrupt() {
    const std::uint8_t interruptFlags = ReadBus(IF);

    WriteBus(
        IF,
        static_cast<std::uint8_t>(interruptFlags | 0x01u)
    );
}

void PPU::RequestSTATInterrupt() {
    const std::uint8_t interruptFlags = ReadBus(IF);

    WriteBus(
        IF,
        static_cast<std::uint8_t>(interruptFlags | 0x02u)
    );
}

void PPU::RenderScanline() {
    if (ly_ >= SCREEN_HEIGHT) {
        return;
    }

    RenderBackgroundScanline();

    if (RenderWindowScanline()) {
        ++windowLine_;
    }

    RenderSpriteScanline();
}

void PPU::RenderBackgroundScanline() {
    const std::uint8_t lcdc = ReadBus(LCDC);
    const std::uint8_t palette = ReadBus(BGP);

    const std::size_t rowStart =
        static_cast<std::size_t>(ly_) * SCREEN_WIDTH;

    // In DMG mode LCDC.0 disables both BG and Window.
    if ((lcdc & 0x01u) == 0) {
        const std::uint8_t shade = ApplyPalette(0, palette);

        for (std::size_t x = 0; x < SCREEN_WIDTH; ++x) {
            bgColorIds_[x] = 0;
            framebuffer_[rowStart + x] = shade;
        }

        return;
    }

    const std::uint8_t scrollX = ReadBus(SCX);
    const std::uint8_t scrollY = ReadBus(SCY);

    // uint8_t conversion intentionally provides 256-pixel wrapping.
    const std::uint8_t backgroundY = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(scrollY) +
        static_cast<std::uint16_t>(ly_)
    );

    const std::uint8_t tileY =
        static_cast<std::uint8_t>(backgroundY / 8u);

    const std::uint8_t pixelY =
        static_cast<std::uint8_t>(backgroundY % 8u);

    const std::uint16_t tileMapBase =
        GetBackgroundTileMapBase(lcdc);

    for (std::size_t screenX = 0;
         screenX < SCREEN_WIDTH;
         ++screenX) {
        const std::uint8_t backgroundX =
            static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(scrollX) +
                static_cast<std::uint16_t>(screenX)
            );

        const std::uint8_t tileX =
            static_cast<std::uint8_t>(backgroundX / 8u);

        const std::uint8_t pixelX =
            static_cast<std::uint8_t>(backgroundX % 8u);

        const std::uint16_t tileMapAddress =
            static_cast<std::uint16_t>(
                tileMapBase +
                static_cast<std::uint16_t>(tileY) * 32u +
                tileX
            );

        const std::uint8_t tileId =
            ReadBus(tileMapAddress);

        const std::uint16_t tileAddress =
            GetTileDataAddress(tileId, lcdc);

        const std::uint8_t colorId =
            DecodeTilePixel(
                tileAddress,
                pixelY,
                pixelX
            );

        bgColorIds_[screenX] = colorId;
        framebuffer_[rowStart + screenX] =
            ApplyPalette(colorId, palette);
    }
}

bool PPU::RenderWindowScanline() {
    const std::uint8_t lcdc = ReadBus(LCDC);

    const bool backgroundWindowEnabled =
        (lcdc & 0x01u) != 0;

    const bool windowEnabled =
        (lcdc & 0x20u) != 0;

    if (!backgroundWindowEnabled || !windowEnabled) {
        return false;
    }

    const std::uint8_t windowYPosition = ReadBus(WY);

    if (ly_ < windowYPosition) {
        return false;
    }

    const std::uint8_t windowXRegister = ReadBus(WX);

    // WX = 167 places the first Window pixel at screen X = 160,
    // so no Window pixel is visible.
    if (windowXRegister > 166u) {
        return false;
    }

    const int windowLeft =
        static_cast<int>(windowXRegister) - 7;

    const int firstVisibleX =
        std::max(0, windowLeft);

    if (firstVisibleX >= static_cast<int>(SCREEN_WIDTH)) {
        return false;
    }

    const std::uint8_t palette = ReadBus(BGP);
    const std::uint16_t tileMapBase =
        GetWindowTileMapBase(lcdc);

    const std::uint8_t windowY = windowLine_;
    const std::uint8_t tileY =
        static_cast<std::uint8_t>(windowY / 8u);
    const std::uint8_t pixelY =
        static_cast<std::uint8_t>(windowY % 8u);

    const std::size_t rowStart =
        static_cast<std::size_t>(ly_) * SCREEN_WIDTH;

    for (int screenX = firstVisibleX;
         screenX < static_cast<int>(SCREEN_WIDTH);
         ++screenX) {
        const int windowX =
            screenX - windowLeft;

        const std::uint8_t tileX =
            static_cast<std::uint8_t>(
                static_cast<unsigned int>(windowX) / 8u
            );

        const std::uint8_t pixelX =
            static_cast<std::uint8_t>(
                static_cast<unsigned int>(windowX) % 8u
            );

        const std::uint16_t tileMapAddress =
            static_cast<std::uint16_t>(
                tileMapBase +
                static_cast<std::uint16_t>(tileY) * 32u +
                tileX
            );

        const std::uint8_t tileId =
            ReadBus(tileMapAddress);

        const std::uint16_t tileAddress =
            GetTileDataAddress(tileId, lcdc);

        const std::uint8_t colorId =
            DecodeTilePixel(
                tileAddress,
                pixelY,
                pixelX
            );

        const std::size_t x =
            static_cast<std::size_t>(screenX);

        bgColorIds_[x] = colorId;
        framebuffer_[rowStart + x] =
            ApplyPalette(colorId, palette);
    }

    return true;
}

void PPU::RenderSpriteScanline() {
    const std::uint8_t lcdc = ReadBus(LCDC);

    if ((lcdc & 0x02u) == 0) {
        return;
    }

    const int spriteHeight =
        (lcdc & 0x04u) != 0 ? 16 : 8;

    std::array<Sprite, MAX_SPRITES_PER_LINE> sprites{};
    std::size_t spriteCount = 0;

    // OAM search keeps the first 10 matching objects in OAM order.
    for (std::size_t i = 0;
         i < OAM_SPRITE_COUNT &&
         spriteCount < MAX_SPRITES_PER_LINE;
         ++i) {
        const std::uint16_t address =
            static_cast<std::uint16_t>(
                OAM_BASE + i * 4u
            );

        Sprite sprite{
            .oamIndex = static_cast<std::uint8_t>(i),
            .y = ReadBus(address),
            .x = ReadBus(
                static_cast<std::uint16_t>(address + 1u)
            ),
            .tileId = ReadBus(
                static_cast<std::uint16_t>(address + 2u)
            ),
            .attributes = ReadBus(
                static_cast<std::uint16_t>(address + 3u)
            ),
        };

        const int spriteTop =
            static_cast<int>(sprite.y) - 16;

        const int currentLine =
            static_cast<int>(ly_);

        if (currentLine >= spriteTop &&
            currentLine < spriteTop + spriteHeight) {
            sprites[spriteCount++] = sprite;
        }
    }

    // On DMG, overlapping object priority is:
    // smaller X first, then smaller OAM index.
    std::sort(
        sprites.begin(),
        sprites.begin() +
            static_cast<std::ptrdiff_t>(spriteCount),
        [](const Sprite& left, const Sprite& right) {
            if (left.x != right.x) {
                return left.x < right.x;
            }

            return left.oamIndex < right.oamIndex;
        }
    );

    const std::size_t rowStart =
        static_cast<std::size_t>(ly_) * SCREEN_WIDTH;

    for (std::size_t screenX = 0;
         screenX < SCREEN_WIDTH;
         ++screenX) {
        const Sprite* winningSprite = nullptr;
        std::uint8_t winningColorId = 0;

        for (std::size_t i = 0; i < spriteCount; ++i) {
            const Sprite& sprite = sprites[i];

            const int spriteLeft =
                static_cast<int>(sprite.x) - 8;

            const int spriteTop =
                static_cast<int>(sprite.y) - 16;

            int spriteX =
                static_cast<int>(screenX) - spriteLeft;

            if (spriteX < 0 || spriteX >= 8) {
                continue;
            }

            int spriteY =
                static_cast<int>(ly_) - spriteTop;

            if ((sprite.attributes & 0x20u) != 0) {
                spriteX = 7 - spriteX;
            }

            if ((sprite.attributes & 0x40u) != 0) {
                spriteY = spriteHeight - 1 - spriteY;
            }

            std::uint8_t tileId = sprite.tileId;
            std::uint8_t tileRow = 0;

            if (spriteHeight == 16) {
                tileId =
                    static_cast<std::uint8_t>(
                        tileId & 0xFEu
                    );

                tileId = static_cast<std::uint8_t>(
                    tileId +
                    static_cast<std::uint8_t>(
                        spriteY / 8
                    )
                );

                tileRow =
                    static_cast<std::uint8_t>(
                        spriteY % 8
                    );
            } else {
                tileRow =
                    static_cast<std::uint8_t>(spriteY);
            }

            const std::uint16_t tileAddress =
                static_cast<std::uint16_t>(
                    TILE_DATA_UNSIGNED_BASE +
                    static_cast<std::uint16_t>(tileId) * 16u
                );

            const std::uint8_t colorId =
                DecodeTilePixel(
                    tileAddress,
                    tileRow,
                    static_cast<std::uint8_t>(spriteX)
                );

            // OBJ color ID 0 is transparent.
            if (colorId == 0) {
                continue;
            }

            winningSprite = &sprite;
            winningColorId = colorId;
            break;
        }

        if (winningSprite == nullptr) {
            continue;
        }

        // Priority is resolved between objects first.
        // Then the winning object is mixed with BG/Window.
        const bool behindBackground =
            (winningSprite->attributes & 0x80u) != 0;

        if (behindBackground &&
            bgColorIds_[screenX] != 0) {
            continue;
        }

        const std::uint8_t palette =
            (winningSprite->attributes & 0x10u) != 0
                ? ReadBus(OBP1)
                : ReadBus(OBP0);

        framebuffer_[rowStart + screenX] =
            ApplyPalette(winningColorId, palette);
    }
}

std::uint16_t PPU::GetBackgroundTileMapBase(
    const std::uint8_t lcdc
) const noexcept {
    return (lcdc & 0x08u) != 0
        ? BG_TILE_MAP_1_BASE
        : BG_TILE_MAP_0_BASE;
}

std::uint16_t PPU::GetWindowTileMapBase(
    const std::uint8_t lcdc
) const noexcept {
    return (lcdc & 0x40u) != 0
        ? BG_TILE_MAP_1_BASE
        : BG_TILE_MAP_0_BASE;
}

std::uint16_t PPU::GetTileDataAddress(
    const std::uint8_t tileId,
    const std::uint8_t lcdc
) const noexcept {
    if ((lcdc & 0x10u) != 0) {
        return static_cast<std::uint16_t>(
            TILE_DATA_UNSIGNED_BASE +
            static_cast<std::uint16_t>(tileId) * 16u
        );
    }

    const std::int16_t signedTileId =
        tileId < 0x80u
            ? static_cast<std::int16_t>(tileId)
            : static_cast<std::int16_t>(
                static_cast<std::int16_t>(tileId) - 256
            );

    const std::int32_t address =
        static_cast<std::int32_t>(TILE_DATA_SIGNED_BASE) +
        static_cast<std::int32_t>(signedTileId) * 16;

    return static_cast<std::uint16_t>(address);
}

std::uint8_t PPU::DecodeTilePixel(
    const std::uint16_t tileAddress,
    const std::uint8_t row,
    const std::uint8_t column
) const {
    const std::uint16_t rowAddress =
        static_cast<std::uint16_t>(
            tileAddress +
            static_cast<std::uint16_t>(row) * 2u
        );

    const std::uint8_t low = ReadBus(rowAddress);
    const std::uint8_t high =
        ReadBus(static_cast<std::uint16_t>(rowAddress + 1u));

    const std::uint8_t bit =
        static_cast<std::uint8_t>(7u - column);

    const std::uint8_t lowBit =
        static_cast<std::uint8_t>((low >> bit) & 0x01u);

    const std::uint8_t highBit =
        static_cast<std::uint8_t>((high >> bit) & 0x01u);

    return static_cast<std::uint8_t>(
        (highBit << 1u) | lowBit
    );
}

std::uint8_t PPU::ApplyPalette(
    const std::uint8_t colorId,
    const std::uint8_t palette
) noexcept {
    const std::uint8_t shift =
        static_cast<std::uint8_t>(colorId * 2u);

    return static_cast<std::uint8_t>(
        (palette >> shift) & 0x03u
    );
}

} // namespace PixelLink::GameBoy
