#include <cstddef>
#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::PPUTest {

namespace {

constexpr uint16_t IF   = 0xFF0F;

constexpr uint16_t LCDC = 0xFF40;
constexpr uint16_t STAT = 0xFF41;
constexpr uint16_t SCY  = 0xFF42;
constexpr uint16_t SCX  = 0xFF43;
constexpr uint16_t LY   = 0xFF44;
constexpr uint16_t LYC  = 0xFF45;
constexpr uint16_t BGP  = 0xFF47;
constexpr uint16_t OBP0 = 0xFF48;
constexpr uint16_t OBP1 = 0xFF49;
constexpr uint16_t WY   = 0xFF4A;
constexpr uint16_t WX   = 0xFF4B;

constexpr uint16_t TILE_DATA_0 = 0x8000;
constexpr uint16_t TILE_DATA_SIGNED_MINUS_1 = 0x8FF0;
constexpr uint16_t BG_MAP_0 = 0x9800;
constexpr uint16_t BG_MAP_1 = 0x9C00;
constexpr uint16_t OAM = 0xFE00;

void enableLCD(Bus& bus) {
    bus.Write(
        LCDC,
        static_cast<uint8_t>(bus.Read(LCDC) | 0x80u)
    );
}

void configureBackground(
    Bus& bus,
    uint8_t lcdc = 0x91
) {
    bus.Write(LCDC, lcdc);
    bus.Write(BGP, 0xE4);
    bus.Write(OBP0, 0xE4);
    bus.Write(OBP1, 0xE4);
    bus.Write(SCX, 0);
    bus.Write(SCY, 0);
    bus.Write(WY, 0);
    bus.Write(WX, 7);
}

void writeTileRow(
    Bus& bus,
    uint16_t tileAddress,
    uint8_t row,
    uint8_t low,
    uint8_t high
) {
    const uint16_t rowAddress =
        static_cast<uint16_t>(
            tileAddress +
            static_cast<uint16_t>(row) * 2u
        );

    bus.Write(rowAddress, low);
    bus.Write(
        static_cast<uint16_t>(rowAddress + 1u),
        high
    );
}

void writeSolidTileRow(
    Bus& bus,
    uint8_t tileId,
    uint8_t row,
    uint8_t colorId
) {
    const uint8_t low =
        (colorId & 0x01u) != 0 ? 0xFF : 0x00;

    const uint8_t high =
        (colorId & 0x02u) != 0 ? 0xFF : 0x00;

    writeTileRow(
        bus,
        static_cast<uint16_t>(
            TILE_DATA_0 +
            static_cast<uint16_t>(tileId) * 16u
        ),
        row,
        low,
        high
    );
}

void writeSolidTile(
    Bus& bus,
    uint8_t tileId,
    uint8_t colorId
) {
    for (uint8_t row = 0; row < 8; ++row) {
        writeSolidTileRow(
            bus,
            tileId,
            row,
            colorId
        );
    }
}

void writeSprite(
    Bus& bus,
    std::size_t index,
    uint8_t y,
    uint8_t x,
    uint8_t tileId,
    uint8_t attributes = 0
) {
    const uint16_t address =
        static_cast<uint16_t>(
            OAM + index * 4u
        );

    bus.Write(address, y);
    bus.Write(
        static_cast<uint16_t>(address + 1u),
        x
    );
    bus.Write(
        static_cast<uint16_t>(address + 2u),
        tileId
    );
    bus.Write(
        static_cast<uint16_t>(address + 3u),
        attributes
    );
}

uint8_t pixel(
    const PPU& ppu,
    std::size_t x,
    std::size_t y
) {
    return ppu.GetFramebuffer()[
        y * PPU::SCREEN_WIDTH + x
    ];
}

void testVisibleScanlineTiming() {
    Bus bus;
    enableLCD(bus);

    PPU ppu(bus);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 0);

    ppu.Step(83);

    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 83);

    ppu.Step(1);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(ppu.GetLineDot() == 84);

    ppu.Step(171);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(ppu.GetLineDot() == 255);

    ppu.Step(1);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 256);

    ppu.Step(199);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 455);

    ppu.Step(1);

    CHECK(ppu.GetLY() == 1);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 0);
    ppu.Step(4);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
}

void testVBlankEntry() {
    Bus bus;
    enableLCD(bus);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    ppu.Step(456u * 144u + 4);

    CHECK(ppu.GetLY() == 144);
    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    CHECK(ppu.GetLineDot() == 4);
    CHECK((bus.Read(IF) & 0x01u) != 0);
}

void testFrameLength() {
    Bus bus;
    enableLCD(bus);

    PPU ppu(bus);

    ppu.Step(456u * 154u);
    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    ppu.Step(4);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 4);
}

void testCoincidenceFlag() {
    Bus bus;
    enableLCD(bus);
    bus.Write(LYC, 1);

    PPU ppu(bus);

    CHECK(bus.Read(LY) == 0);
    CHECK((bus.Read(STAT) & 0x04u) == 0);

    ppu.Step(456);
    CHECK(ppu.GetLY() == 1);
    CHECK((bus.Read(STAT) & 0x04u) == 0);
    ppu.Step(4);

    CHECK(ppu.GetLY() == 1);
    CHECK(bus.Read(LY) == 1);
    CHECK((bus.Read(STAT) & 0x04u) != 0);
}

void testLCDDisabledState() {
    Bus bus;
    bus.Write(LCDC, 0x00);

    PPU ppu(bus);

    ppu.Step(1000);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 0);
    CHECK(ppu.GetWindowLine() == 0);
    CHECK(bus.Read(LY) == 0);
}

void testLCDEnableAfterDisabled() {
    Bus bus;
    bus.Write(LCDC, 0x00);

    PPU ppu(bus);

    ppu.Step(100);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 0);

    enableLCD(bus);

    ppu.Step(1);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 1);
    CHECK(ppu.GetLY() == 0);
    ppu.Step(76);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    ppu.Step(1);
    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
}

void testSTATMode0Interrupt() {
    Bus bus;
    configureBackground(bus);

    // STAT bit 3 enables the Mode 0 source.
    bus.Write(STAT, 0x08);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    ppu.Step(256);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK((bus.Read(IF) & 0x02u) != 0);
}

void testSTATMode1Interrupt() {
    Bus bus;
    configureBackground(bus);

    // STAT bit 4 enables the Mode 1 source.
    bus.Write(STAT, 0x10);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    ppu.Step(456u * 144u + 4);

    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    CHECK((bus.Read(IF) & 0x02u) != 0);
}

void testSTATMode2Interrupt() {
    Bus bus;
    configureBackground(bus);

    // STAT bit 5 enables the Mode 2 source.
    bus.Write(STAT, 0x20);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK((bus.Read(IF) & 0x02u) != 0);
}

void testSTATLYCInterrupt() {
    Bus bus;
    configureBackground(bus);

    // STAT bit 6 enables the LY == LYC source.
    bus.Write(STAT, 0x40);
    bus.Write(LYC, 1);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    CHECK((bus.Read(IF) & 0x02u) == 0);

    ppu.Step(456);
    CHECK(ppu.GetLY() == 1);
    CHECK((bus.Read(STAT) & 0x04u) == 0);
    ppu.Step(4);

    CHECK(ppu.GetLY() == 1);
    CHECK((bus.Read(STAT) & 0x04u) != 0);
    CHECK((bus.Read(IF) & 0x02u) != 0);
}

void testSTATSharedLineRisingEdge() {
    Bus bus;
    configureBackground(bus);

    // Enable both Mode 0 and Mode 2 sources.
    bus.Write(STAT, 0x28);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    // Construction starts in Mode 2, so the shared STAT line rises.
    CHECK((bus.Read(IF) & 0x02u) != 0);

    // Mode 2 -> Mode 3 makes the line low.
    ppu.Step(84);
    CHECK(ppu.GetMode() == PPU::Mode::Drawing);

    bus.Write(IF, 0x00);

    // Mode 3 -> Mode 0 raises the line again.
    ppu.Step(172);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK((bus.Read(IF) & 0x02u) != 0);

    // Clear IF while the shared line remains high.
    bus.Write(IF, 0x00);

    // Mode 0 -> Mode 2 keeps the ORed STAT line high.
    // There must not be another interrupt request.
    ppu.Step(204);

    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK((bus.Read(IF) & 0x02u) == 0);
}

void testSTATBit7AndReadOnlyStatusBits() {
    Bus bus;
    configureBackground(bus);

    // Pretend the CPU attempted to write all STAT bits.
    bus.Write(STAT, 0xFF);

    PPU ppu(bus);

    const uint8_t stat = bus.Read(STAT);

    CHECK((stat & 0x80u) != 0);
    CHECK((stat & 0x03u) ==
          static_cast<uint8_t>(PPU::Mode::OAMScan));
}

void testCPUAccessRules() {
    Bus bus;
    configureBackground(bus);

    PPU ppu(bus);

    // Mode 2: VRAM is accessible, OAM is blocked.
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.CanCPUAccessVRAM());
    CHECK(!ppu.CanCPUAccessOAM());

    // Mode 3: both VRAM and OAM are blocked.
    ppu.Step(84);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(!ppu.CanCPUAccessVRAM());
    CHECK(!ppu.CanCPUAccessOAM());

    // Mode 0: both are accessible.
    ppu.Step(172);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.CanCPUAccessVRAM());
    CHECK(ppu.CanCPUAccessOAM());

    // Mode 1: both are accessible.
    ppu.Step(
        456u * 144u - 252u
    );

    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    CHECK(ppu.CanCPUAccessVRAM());
    CHECK(ppu.CanCPUAccessOAM());
}

void testCPUAccessRulesLCDDisabled() {
    Bus bus;
    bus.Write(LCDC, 0x00);

    PPU ppu(bus);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.CanCPUAccessVRAM());
    CHECK(ppu.CanCPUAccessOAM());
}

void testBackgroundTileDecoding() {
    Bus bus;
    configureBackground(bus);

    bus.Write(BG_MAP_0, 0x00);
    writeTileRow(
        bus,
        TILE_DATA_0,
        0,
        0b01010101,
        0b00110011
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 0);
    CHECK(pixel(ppu, 1, 0) == 1);
    CHECK(pixel(ppu, 2, 0) == 2);
    CHECK(pixel(ppu, 3, 0) == 3);
    CHECK(pixel(ppu, 4, 0) == 0);
    CHECK(pixel(ppu, 5, 0) == 1);
    CHECK(pixel(ppu, 6, 0) == 2);
    CHECK(pixel(ppu, 7, 0) == 3);
}

void testBackgroundPalette() {
    Bus bus;
    configureBackground(bus);

    bus.Write(BGP, 0x1B);
    bus.Write(BG_MAP_0, 0x00);

    writeTileRow(
        bus,
        TILE_DATA_0,
        0,
        0b01010101,
        0b00110011
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 3);
    CHECK(pixel(ppu, 1, 0) == 2);
    CHECK(pixel(ppu, 2, 0) == 1);
    CHECK(pixel(ppu, 3, 0) == 0);
}

void testBackgroundHorizontalScroll() {
    Bus bus;
    configureBackground(bus);

    writeSolidTileRow(bus, 0, 0, 1);
    writeSolidTileRow(bus, 1, 0, 2);

    bus.Write(
        static_cast<uint16_t>(BG_MAP_0 + 0u),
        0
    );
    bus.Write(
        static_cast<uint16_t>(BG_MAP_0 + 1u),
        1
    );
    bus.Write(
        static_cast<uint16_t>(BG_MAP_0 + 2u),
        0
    );

    bus.Write(SCX, 8);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 7, 0) == 2);
    CHECK(pixel(ppu, 8, 0) == 1);
}

void testBackgroundVerticalScroll() {
    Bus bus;
    configureBackground(bus);

    writeSolidTileRow(bus, 1, 0, 3);

    bus.Write(
        static_cast<uint16_t>(BG_MAP_0 + 32u),
        1
    );
    bus.Write(SCY, 8);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 3);
}

void testBackgroundTileMapSelection() {
    Bus bus;

    configureBackground(bus, 0x99);

    bus.Write(BG_MAP_0, 0);
    bus.Write(BG_MAP_1, 1);

    writeSolidTileRow(bus, 0, 0, 1);
    writeSolidTileRow(bus, 1, 0, 2);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
}

void testSignedTileDataAddressing() {
    Bus bus;

    configureBackground(bus, 0x81);

    bus.Write(BG_MAP_0, 0xFF);

    writeTileRow(
        bus,
        TILE_DATA_SIGNED_MINUS_1,
        0,
        0x00,
        0xFF
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
}

void testBackgroundDisabled() {
    Bus bus;

    configureBackground(bus, 0x90);

    bus.Write(BGP, 0x03);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 3);
    CHECK(pixel(ppu, 80, 0) == 3);
    CHECK(pixel(ppu, 159, 0) == 3);
}

void testWindowOverlay() {
    Bus bus;

    // LCD on, Window on, Window map 1, unsigned tiles, BG on.
    configureBackground(bus, 0xF1);

    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);

    bus.Write(BG_MAP_0, 0);
    bus.Write(BG_MAP_1, 1);

    bus.Write(WY, 0);
    bus.Write(WX, 7);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 7, 0) == 2);
    CHECK(ppu.GetWindowLine() == 1);
}

void testWindowWXOffset() {
    Bus bus;

    configureBackground(bus, 0xF1);

    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);

    bus.Write(BG_MAP_0, 0);
    bus.Write(BG_MAP_1, 1);

    // Window left edge = WX - 7 = 8.
    bus.Write(WY, 0);
    bus.Write(WX, 15);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 7, 0) == 1);
    CHECK(pixel(ppu, 8, 0) == 2);
}

void testWindowWYPosition() {
    Bus bus;

    configureBackground(bus, 0xF1);

    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);

    bus.Write(BG_MAP_0, 0);
    bus.Write(BG_MAP_1, 1);

    bus.Write(WY, 1);
    bus.Write(WX, 7);

    PPU ppu(bus);

    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(ppu.GetWindowLine() == 0);

    ppu.Step(456);

    CHECK(pixel(ppu, 0, 1) == 2);
    CHECK(ppu.GetWindowLine() == 1);
}

void testWindowInternalLineCounter() {
    Bus bus;

    configureBackground(bus, 0xF1);

    bus.Write(BG_MAP_0, 0);
    bus.Write(BG_MAP_1, 1);

    writeSolidTile(bus, 0, 0);

    // Window tile row 0 -> color 1
    // Window tile row 1 -> color 2
    // Window tile row 2 -> color 3
    writeSolidTileRow(bus, 1, 0, 1);
    writeSolidTileRow(bus, 1, 1, 2);
    writeSolidTileRow(bus, 1, 2, 3);

    bus.Write(WY, 0);
    bus.Write(WX, 7);

    PPU ppu(bus);

    // Line 0 renders Window line 0.
    ppu.Step(376);
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(ppu.GetWindowLine() == 1);

    // Disable Window before line 1 is rendered.
    bus.Write(
        LCDC,
        static_cast<uint8_t>(
            bus.Read(LCDC) & ~0x20u
        )
    );

    ppu.Step(456);

    CHECK(pixel(ppu, 0, 1) == 0);
    CHECK(ppu.GetWindowLine() == 1);

    // Re-enable Window. Line 2 must use Window line 1,
    // not line 2.
    bus.Write(
        LCDC,
        static_cast<uint8_t>(
            bus.Read(LCDC) | 0x20u
        )
    );

    ppu.Step(456);

    CHECK(pixel(ppu, 0, 2) == 2);
    CHECK(ppu.GetWindowLine() == 2);
}

void testWindowInvisibleWXDoesNotAdvanceCounter() {
    Bus bus;

    configureBackground(bus, 0xF1);

    bus.Write(WY, 0);
    bus.Write(WX, 167);

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(ppu.GetWindowLine() == 0);
}

void testSpriteBasicRendering() {
    Bus bus;

    // LCD on, OBJ on, unsigned BG tiles, BG on.
    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 2);

    bus.Write(BG_MAP_0, 0);

    writeSprite(
        bus,
        0,
        16,
        8,
        1
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 7, 0) == 2);
}

void testSpriteColorZeroTransparent() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 1);
    bus.Write(BG_MAP_0, 0);

    // Sprite pixel 0 is transparent, pixel 1 is color 2.
    writeTileRow(
        bus,
        static_cast<uint16_t>(TILE_DATA_0 + 16u),
        0,
        0b01000000,
        0b01000000
    );

    writeSprite(
        bus,
        0,
        16,
        8,
        1
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 1, 0) == 3);
}

void testSpritePaletteSelection() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 1);
    bus.Write(BG_MAP_0, 0);

    bus.Write(OBP0, 0xE4);

    // OBP1 maps color ID 1 to shade 3.
    bus.Write(OBP1, 0x0C);

    writeSprite(
        bus,
        0,
        16,
        8,
        1,
        0x10
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 3);
}

void testSpriteXFlip() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    bus.Write(BG_MAP_0, 0);

    // Original row:
    // leftmost pixel = color 1
    // rightmost pixel = color 2
    writeTileRow(
        bus,
        static_cast<uint16_t>(TILE_DATA_0 + 16u),
        0,
        0b10000000,
        0b00000001
    );

    writeSprite(
        bus,
        0,
        16,
        8,
        1,
        0x20
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 7, 0) == 1);
}

void testSpriteYFlip() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    bus.Write(BG_MAP_0, 0);

    writeSolidTileRow(bus, 1, 0, 1);
    writeSolidTileRow(bus, 1, 7, 3);

    writeSprite(
        bus,
        0,
        16,
        8,
        1,
        0x40
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 3);
}

void testSpriteBehindBackgroundPriority() {
    Bus bus;

    configureBackground(bus, 0x93);

    // BG x=0 has color 1, x=1 has color 0.
    writeTileRow(
        bus,
        TILE_DATA_0,
        0,
        0b10000000,
        0b00000000
    );

    bus.Write(BG_MAP_0, 0);

    writeSolidTile(bus, 1, 2);

    writeSprite(
        bus,
        0,
        16,
        8,
        1,
        0x80
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 1, 0) == 2);
}

void testSpritePriorityLowerXWins() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 1);
    writeSolidTile(bus, 2, 2);
    bus.Write(BG_MAP_0, 0);

    // OAM 0 starts at screen X=2.
    writeSprite(
        bus,
        0,
        16,
        10,
        1
    );

    // OAM 1 starts at screen X=0 and therefore has
    // higher DMG object priority where they overlap.
    writeSprite(
        bus,
        1,
        16,
        8,
        2
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 2, 0) == 2);
}

void testSpritePrioritySameXLowerOAMWins() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 1);
    writeSolidTile(bus, 2, 2);
    bus.Write(BG_MAP_0, 0);

    writeSprite(
        bus,
        0,
        16,
        8,
        1
    );

    writeSprite(
        bus,
        1,
        16,
        8,
        2
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 1);
}

void testSpriteTenPerLineLimit() {
    Bus bus;

    configureBackground(bus, 0x93);

    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 3);
    bus.Write(BG_MAP_0, 0);

    // The first 10 matching OAM entries are horizontally hidden,
    // but still consume the 10-object scanline limit.
    for (std::size_t i = 0; i < 10; ++i) {
        writeSprite(
            bus,
            i,
            16,
            0,
            1
        );
    }

    // This visible 11th object must not be selected.
    writeSprite(
        bus,
        10,
        16,
        8,
        1
    );

    PPU ppu(bus);
    ppu.Step(376);

    CHECK(pixel(ppu, 0, 0) == 0);
}

void testSprite8x16Mode() {
    Bus bus;

    // LCD on, OBJ on, 8x16 OBJ, unsigned BG tiles, BG on.
    configureBackground(bus, 0x97);

    writeSolidTile(bus, 0, 0);
    bus.Write(BG_MAP_0, 0);

    // Tile ID 3 is intentionally odd.
    // 8x16 mode must ignore bit 0:
    // top tile = 2, bottom tile = 3.
    writeSolidTile(bus, 2, 1);
    writeSolidTile(bus, 3, 2);

    writeSprite(
        bus,
        0,
        16,
        8,
        3
    );

    PPU ppu(bus);

    ppu.Step(376);
    CHECK(pixel(ppu, 0, 0) == 1);

    ppu.Step(456u * 8u);
    CHECK(pixel(ppu, 0, 8) == 2);
}

void testSprite8x16YFlip() {
    Bus bus;

    configureBackground(bus, 0x97);

    writeSolidTile(bus, 0, 0);
    bus.Write(BG_MAP_0, 0);

    writeSolidTile(bus, 2, 1);
    writeSolidTile(bus, 3, 2);

    writeSprite(
        bus,
        0,
        16,
        8,
        2,
        0x40
    );

    PPU ppu(bus);
    ppu.Step(376);

    // Y-flipping the whole 8x16 object makes screen line 0
    // read from the bottom tile.
    CHECK(pixel(ppu, 0, 0) == 2);
}

void testSeparateReadWriteBoundaries() {
    Bus bus;
    bus.Write(LCDC, 0);
    bus.Write(OAM, 0x12);
    bus.Write(TILE_DATA_0, 0x34);
    PPU ppu(bus);
    enableLCD(bus);
    ppu.Step(77);
    CHECK(bus.Read(TILE_DATA_0) == 0x34);
    ppu.Step(1);
    CHECK(bus.Read(TILE_DATA_0) == 0xFF);

    // The shortened first line ends at 450 dots. On the next line
    // OAM reads close before Mode 2, but writes remain open until dot 4.
    ppu.Step(374);
    CHECK(ppu.GetLY() == 1);
    CHECK(ppu.GetLineDot() == 2);
    CHECK(bus.Read(OAM) == 0xFF);
    bus.Write(OAM, 0x56);
    CHECK(bus.Read(OAM, BusAccess::Internal) == 0x56);
    ppu.Step(2);
    bus.Write(OAM, 0x78);
    CHECK(bus.Read(OAM, BusAccess::Internal) == 0x56);

    ppu.Step(78);
    CHECK(ppu.GetLineDot() == 82);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(bus.Read(TILE_DATA_0) == 0xFF);
    CHECK(bus.Read(OAM) == 0xFF);
    bus.Write(OAM, 0x78); // OAM write gate briefly reopens at search end
    CHECK(bus.Read(OAM, BusAccess::Internal) == 0x78);
    bus.Write(TILE_DATA_0, 0x9A);
    CHECK(bus.Read(TILE_DATA_0, BusAccess::Internal) == 0x9A);
    ppu.Step(2);
    bus.Write(OAM, 0xBC);
    CHECK(bus.Read(OAM, BusAccess::Internal) == 0x78);
    bus.Write(TILE_DATA_0, 0xBC);
    CHECK(bus.Read(TILE_DATA_0, BusAccess::Internal) == 0x9A);
}

void testFinalVBlankLineLY() {
    Bus bus;
    PPU ppu(bus);
    ppu.Step(153u * 456u + 3u);
    CHECK(ppu.GetLY() == 153);
    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    ppu.Step(1);
    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    bus.Write(LY, 99);
    CHECK(bus.Read(LY) == 0);
    ppu.Step(452);
    CHECK(ppu.GetLineDot() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    ppu.Step(4);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
}

void testFIFOProgressiveOutputAndLivePalette() {
    Bus bus;
    configureBackground(bus);
    writeSolidTile(bus, 0, 1);
    PPU ppu(bus);
    ppu.Step(96);
    CHECK(pixel(ppu, 0, 0) == 0);
    ppu.Step(1);
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 1, 0) == 0);
    ppu.Step(39); // First 40 pixels have already reached the LCD.
    bus.Write(BGP, 0x0C); // Map color 1 to shade 3.
    ppu.Step(120);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    for (std::size_t x = 0; x < 160; ++x) {
        CHECK(pixel(ppu, x, 0) == (x < 40 ? 1 : 3));
    }
}

void testFIFOFineScrollAndTransferCompletion() {
    for (uint8_t fine = 0; fine < 8; ++fine) {
        Bus bus;
        configureBackground(bus);
        bus.Write(SCX, fine);
        writeTileRow(bus, TILE_DATA_0, 0, 0x55, 0x33);
        PPU ppu(bus);
        ppu.Step(255 + fine);
        CHECK(ppu.GetMode() == PPU::Mode::Drawing);
        CHECK(pixel(ppu, 0, 0) == fine % 4);
        ppu.Step(1);
        CHECK(ppu.GetMode() == PPU::Mode::HBlank);
        CHECK(pixel(ppu, 159, 0) == (159 + fine) % 4);
    }
}

void testFIFOBytesLatchedBeforePixelOutput() {
    Bus bus;
    configureBackground(bus);
    writeSolidTile(bus, 0, 1);
    PPU ppu(bus);
    ppu.Step(96); // Both planes of the first visible tile have been fetched.
    writeTileRow(bus, TILE_DATA_0, 0, 0, 0); // CPU writes are blocked in Mode 3.
    CHECK(bus.Read(TILE_DATA_0, BusAccess::Internal) == 0xFF);
    bus.Write(TILE_DATA_0, 0, BusAccess::Internal);
    ppu.Step(160);
    for (std::size_t x = 0; x < 160; ++x) {
        CHECK(pixel(ppu, x, 0) == (x < 8 ? 1 : 0));
    }
}

void testFIFOSeparatePlaneReads() {
    Bus bus;
    configureBackground(bus);
    writeTileRow(bus, TILE_DATA_0, 0, 0xFF, 0);
    writeTileRow(bus, TILE_DATA_0, 1, 0, 0xFF);
    PPU ppu(bus);
    ppu.Step(94); // Low plane of the visible tile uses SCY=0.
    bus.Write(SCY, 1);
    ppu.Step(162);
    CHECK(pixel(ppu, 0, 0) == 3); // High plane is fetched later using SCY=1.
    CHECK(pixel(ppu, 7, 0) == 3);
    CHECK(pixel(ppu, 8, 0) == 2);
}

void testFIFOLiveTileMapAndCoarseScroll() {
    for (const bool changeMap : {false, true}) {
        Bus bus;
        configureBackground(bus);
        writeSolidTile(bus, 0, 1);
        writeSolidTile(bus, 1, 2);
        for (int i = 0; i < 32; ++i) {
            bus.Write(static_cast<uint16_t>(BG_MAP_0 + i), static_cast<uint8_t>(i & 1));
            bus.Write(static_cast<uint16_t>(BG_MAP_1 + i), 1);
        }
        PPU ppu(bus);
        ppu.Step(128);
        CHECK(pixel(ppu, 0, 0) == 1);
        if (changeMap) bus.Write(LCDC, 0x99);
        else bus.Write(SCX, 8);
        ppu.Step(128);
        CHECK(pixel(ppu, 0, 0) == 1); // A later write cannot repaint old pixels.
        CHECK(pixel(ppu, 80, 0) == 2);
    }
}

void testFIFOWindowRestartAndClipping() {
    for (uint8_t wx = 0; wx < 16; ++wx) {
        Bus bus;
        configureBackground(bus, 0xF1);
        bus.Write(WX, wx);
        writeSolidTile(bus, 0, 0);
        writeSolidTile(bus, 1, 1);
        bus.Write(BG_MAP_1, 1);
        writeTileRow(bus, static_cast<uint16_t>(TILE_DATA_0 + 16), 0, 0x55, 0x33);
        PPU ppu(bus);
        ppu.Step(376);
        CHECK(ppu.GetWindowLine() == 1);
        for (int x = 0; x < 8; ++x) {
            const int windowX = x - (static_cast<int>(wx) - 7);
            CHECK(pixel(ppu, x, 0) == (windowX < 0 || windowX >= 8 ? 0 : windowX % 4));
        }
    }
}

void testFIFOWindowStartsDuringDrawing() {
    Bus bus;
    configureBackground(bus, 0xD1); // Window initially disabled.
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);
    for (int i = 0; i < 32; ++i) bus.Write(static_cast<uint16_t>(BG_MAP_1 + i), 1);
    bus.Write(WX, 47);
    PPU ppu(bus);
    ppu.Step(128);
    bus.Write(LCDC, 0xF1);
    ppu.Step(133); // Baseline HBlank + six-dot window restart, less one dot.
    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    ppu.Step(1);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(pixel(ppu, 39, 0) == 1);
    CHECK(pixel(ppu, 40, 0) == 2);
    CHECK(pixel(ppu, 159, 0) == 2);
}

void testFIFOWindowYComparisonIsLatched() {
    Bus bus;
    configureBackground(bus, 0xF1);
    bus.Write(WY, 1);
    PPU ppu(bus);
    ppu.Step(456 + 4); // WY=LY during OAM search triggers the window this frame.
    bus.Write(WY, 100);
    ppu.Step(372);
    CHECK(ppu.GetWindowLine() == 1);
    ppu.Step(456);
    CHECK(ppu.GetWindowLine() == 2);
}

void testFIFOObjectPaletteChangesDuringDrawing() {
    Bus bus;
    configureBackground(bus, 0x93);
    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 1);
    writeSprite(bus, 0, 16, 8, 1);
    PPU ppu(bus);
    // Wait for the first object pixel, allowing for its fetch stall.
    for (int dots = 0; dots < 150 && pixel(ppu, 0, 0) == 0; ++dots) ppu.Step(1);
    CHECK(pixel(ppu, 0, 0) == 1);
    bus.Write(OBP0, 0x0C);
    ppu.Step(376 - ppu.GetLineDot());
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 1, 0) == 3);
    CHECK(pixel(ppu, 7, 0) == 3);
}

void testFIFOOAMSearchLatchesCoordinates() {
    Bus bus;
    configureBackground(bus, 0x93);
    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 2);
    writeSprite(bus, 0, 16, 8, 1);
    PPU ppu(bus);
    ppu.Step(84);
    bus.Write(OAM + 1, 80, BusAccess::Internal); // Search result must survive later OAM changes.
    ppu.Step(292);
    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 72, 0) == 0);
}

void testFIFOLCDDisableFlushesPendingPixels() {
    Bus bus;
    configureBackground(bus, 0xF3);
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 3);
    writeSprite(bus, 0, 16, 8, 1);
    PPU ppu(bus);
    ppu.Step(130);
    bus.Write(LCDC, 0);
    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetLineDot() == 0);
    CHECK(ppu.GetWindowLine() == 0);
    CHECK(pixel(ppu, 0, 0) == 0);
    writeSolidTile(bus, 0, 2);
    bus.Write(LCDC, 0x91);
    ppu.Step(376);
    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 159, 0) == 2);
    CHECK(ppu.GetWindowLine() == 0);
}

void testFIFOWindowDisableResumesBackground() {
    Bus bus;
    configureBackground(bus, 0xF1);
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);
    for (int i = 0; i < 32; ++i) bus.Write(static_cast<uint16_t>(BG_MAP_1 + i), 1);
    PPU ppu(bus);
    ppu.Step(142); // 40 window pixels have been emitted after the six-dot restart.
    CHECK(pixel(ppu, 39, 0) == 2);
    bus.Write(LCDC, 0xD1);
    ppu.Step(234);
    CHECK(pixel(ppu, 0, 0) == 2);
    CHECK(pixel(ppu, 40, 0) == 1);
    CHECK(pixel(ppu, 159, 0) == 1);
    CHECK(ppu.GetWindowLine() == 1);
}

void testFIFOWindowDoesNotTriggerBehindOutput() {
    Bus bus;
    configureBackground(bus, 0xD1);
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);
    bus.Write(BG_MAP_1, 1);
    PPU ppu(bus);
    ppu.Step(128);
    bus.Write(LCDC, 0xF1); // WX=7's comparison has already passed this line.
    ppu.Step(248);
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 32, 0) == 1);
    CHECK(ppu.GetWindowLine() == 0);
    ppu.Step(456);
    CHECK(pixel(ppu, 0, 1) == 2);
    CHECK(ppu.GetWindowLine() == 1);
}

void testFIFOObjectFetchCancellation() {
    Bus bus;
    configureBackground(bus, 0x93);
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);
    writeSprite(bus, 0, 16, 8, 1);
    PPU ppu(bus);
    ppu.Step(103); // During the object's tile fetch, before any LCD pixel.
    bus.Write(LCDC, 0x91);
    ppu.Step(273);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 7, 0) == 1);
    CHECK(pixel(ppu, 159, 0) == 1);
}

void testFIFOObjectTileAndByteLatches() {
    Bus bus;
    configureBackground(bus, 0x93);
    writeSolidTile(bus, 0, 0);
    writeSolidTile(bus, 1, 1);
    writeSolidTile(bus, 2, 2);
    writeSprite(bus, 0, 16, 8, 1);
    PPU ppu(bus);
    ppu.Step(84);
    bus.Write(OAM + 2, 2, BusAccess::Internal); // Tile is read during fetch, after OAM search.
    for (int dots = 0; dots < 100 && pixel(ppu, 0, 0) == 0; ++dots) ppu.Step(1);
    CHECK(pixel(ppu, 0, 0) == 2);
    bus.Write(TILE_DATA_0 + 2 * 16 + 1, 0, BusAccess::Internal);
    ppu.Step(376 - ppu.GetLineDot());
    CHECK(pixel(ppu, 7, 0) == 2); // Already queued sprite pixels keep their fetched bytes.
}

void testFIFOObjectClippingAndOcclusion() {
    Bus bus;
    configureBackground(bus, 0x93);
    writeSolidTile(bus, 0, 1);
    writeSolidTile(bus, 1, 2);
    writeSolidTile(bus, 2, 3);
    writeSprite(bus, 0, 16, 4, 1, 0x80); // Winner is behind nonzero BG.
    writeSprite(bus, 1, 16, 4, 2);       // Must not show through the winning object.
    writeSprite(bus, 2, 16, 167, 2);     // Only its first pixel is on-screen.
    PPU ppu(bus);
    ppu.Step(376);
    CHECK(pixel(ppu, 0, 0) == 1);
    CHECK(pixel(ppu, 3, 0) == 1);
    CHECK(pixel(ppu, 158, 0) == 1);
    CHECK(pixel(ppu, 159, 0) == 3);
}

void testFIFOChunkIndependentStepping() {
    Bus batchedBus, singleBus;
    for (auto* bus : {&batchedBus, &singleBus}) {
        configureBackground(*bus, 0xF7);
        bus->Write(SCX, 5);
        bus->Write(WX, 63);
        writeSolidTile(*bus, 0, 1);
        writeSolidTile(*bus, 1, 2);
        for (int i = 0; i < 32; ++i) bus->Write(static_cast<uint16_t>(BG_MAP_1 + i), 1);
        for (int i = 0; i < 10; ++i) writeSprite(*bus, i, 16, static_cast<uint8_t>(i * 15), 1);
    }
    PPU batched(batchedBus), single(singleBus);
    for (int line = 0; line < 154; ++line) {
        batched.Step(456);
        for (int dot = 0; dot < 456; ++dot) single.Step(1);
        CHECK(batched.GetFramebuffer() == single.GetFramebuffer());
        CHECK(batched.GetMode() == single.GetMode());
        CHECK(batched.GetLY() == single.GetLY());
        CHECK(batched.GetWindowLine() == single.GetWindowLine());
    }
}

} // namespace

void run() {
    Test::run("PPU FIFO / progressive output and live BG palette", testFIFOProgressiveOutputAndLivePalette);
    Test::run("PPU FIFO / fine scroll and last-pixel HBlank", testFIFOFineScrollAndTransferCompletion);
    Test::run("PPU FIFO / fetched bytes survive later VRAM writes", testFIFOBytesLatchedBeforePixelOutput);
    Test::run("PPU FIFO / separate low and high plane fetches", testFIFOSeparatePlaneReads);
    Test::run("PPU FIFO / live tile map and coarse scroll", testFIFOLiveTileMapAndCoarseScroll);
    Test::run("PPU FIFO / window restart and left clipping", testFIFOWindowRestartAndClipping);
    Test::run("PPU FIFO / window enable during drawing", testFIFOWindowStartsDuringDrawing);
    Test::run("PPU FIFO / WY comparison latch", testFIFOWindowYComparisonIsLatched);
    Test::run("PPU FIFO / live OBJ palette", testFIFOObjectPaletteChangesDuringDrawing);
    Test::run("PPU FIFO / OAM search coordinate latch", testFIFOOAMSearchLatchesCoordinates);
    Test::run("PPU FIFO / LCD disable flush and restart", testFIFOLCDDisableFlushesPendingPixels);
    Test::run("PPU FIFO / window disable resumes BG", testFIFOWindowDisableResumesBackground);
    Test::run("PPU FIFO / window edge already passed", testFIFOWindowDoesNotTriggerBehindOutput);
    Test::run("PPU FIFO / OBJ fetch cancellation", testFIFOObjectFetchCancellation);
    Test::run("PPU FIFO / OBJ tile and byte latches", testFIFOObjectTileAndByteLatches);
    Test::run("PPU FIFO / clipped objects and BG occlusion", testFIFOObjectClippingAndOcclusion);
    Test::run("PPU FIFO / step chunk invariance over a frame", testFIFOChunkIndependentStepping);
    Test::run("PPU / separate read and write port boundaries", testSeparateReadWriteBoundaries);
    Test::run("PPU / final VBlank line LY and read-only register", testFinalVBlankLineLY);
    Test::run("PPU / visible scanline timing", testVisibleScanlineTiming);
    Test::run("PPU / VBlank entry", testVBlankEntry);
    Test::run("PPU / frame length", testFrameLength);
    Test::run("PPU / LY / LYC coincidence", testCoincidenceFlag);
    Test::run("PPU / LCD disabled state", testLCDDisabledState);
    Test::run("PPU / LCD enable", testLCDEnableAfterDisabled);

    Test::run("PPU / STAT Mode 0 IRQ", testSTATMode0Interrupt);
    Test::run("PPU / STAT Mode 1 IRQ", testSTATMode1Interrupt);
    Test::run("PPU / STAT Mode 2 IRQ", testSTATMode2Interrupt);
    Test::run("PPU / STAT LYC IRQ", testSTATLYCInterrupt);
    Test::run(
        "PPU / STAT shared-line rising edge",
        testSTATSharedLineRisingEdge
    );
    Test::run(
        "PPU / STAT status bits",
        testSTATBit7AndReadOnlyStatusBits
    );
    Test::run("PPU / CPU VRAM/OAM access rules", testCPUAccessRules);
    Test::run(
        "PPU / CPU access with LCD disabled",
        testCPUAccessRulesLCDDisabled
    );

    Test::run("PPU / BG tile decoding", testBackgroundTileDecoding);
    Test::run("PPU / BG palette", testBackgroundPalette);
    Test::run("PPU / BG horizontal scroll", testBackgroundHorizontalScroll);
    Test::run("PPU / BG vertical scroll", testBackgroundVerticalScroll);
    Test::run("PPU / BG tile map selection", testBackgroundTileMapSelection);
    Test::run("PPU / BG signed tile addressing", testSignedTileDataAddressing);
    Test::run("PPU / BG disabled", testBackgroundDisabled);

    Test::run("PPU / Window overlay", testWindowOverlay);
    Test::run("PPU / Window WX offset", testWindowWXOffset);
    Test::run("PPU / Window WY position", testWindowWYPosition);
    Test::run("PPU / Window internal line counter", testWindowInternalLineCounter);
    Test::run(
        "PPU / Window invisible WX",
        testWindowInvisibleWXDoesNotAdvanceCounter
    );

    Test::run("PPU / OBJ basic rendering", testSpriteBasicRendering);
    Test::run("PPU / OBJ color 0 transparent", testSpriteColorZeroTransparent);
    Test::run("PPU / OBJ palette selection", testSpritePaletteSelection);
    Test::run("PPU / OBJ X flip", testSpriteXFlip);
    Test::run("PPU / OBJ Y flip", testSpriteYFlip);
    Test::run("PPU / OBJ behind BG priority", testSpriteBehindBackgroundPriority);
    Test::run("PPU / OBJ lower X priority", testSpritePriorityLowerXWins);
    Test::run("PPU / OBJ same X OAM priority", testSpritePrioritySameXLowerOAMWins);
    Test::run("PPU / OBJ 10 per line limit", testSpriteTenPerLineLimit);
    Test::run("PPU / OBJ 8x16 mode", testSprite8x16Mode);
    Test::run("PPU / OBJ 8x16 Y flip", testSprite8x16YFlip);
}

} // namespace PixelLink::Test::GameBoy::PPUTest
