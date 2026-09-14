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

    ppu.Step(79);

    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 79);

    ppu.Step(1);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(ppu.GetLineDot() == 80);

    ppu.Step(171);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(ppu.GetLineDot() == 251);

    ppu.Step(1);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 252);

    ppu.Step(203);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 455);

    ppu.Step(1);

    CHECK(ppu.GetLY() == 1);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 0);
}

void testVBlankEntry() {
    Bus bus;
    enableLCD(bus);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    ppu.Step(456u * 144u);

    CHECK(ppu.GetLY() == 144);
    CHECK(ppu.GetMode() == PPU::Mode::VBlank);
    CHECK(ppu.GetLineDot() == 0);
    CHECK((bus.Read(IF) & 0x01u) != 0);
}

void testFrameLength() {
    Bus bus;
    enableLCD(bus);

    PPU ppu(bus);

    ppu.Step(456u * 154u);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 0);
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

    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(ppu.GetLineDot() == 1);
    CHECK(ppu.GetLY() == 0);
}

void testSTATMode0Interrupt() {
    Bus bus;
    configureBackground(bus);

    // STAT bit 3 enables the Mode 0 source.
    bus.Write(STAT, 0x08);
    bus.Write(IF, 0x00);

    PPU ppu(bus);

    ppu.Step(252);

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

    ppu.Step(456u * 144u);

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
    ppu.Step(80);
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
    ppu.Step(80);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

    CHECK(pixel(ppu, 0, 0) == 2);
}

void testBackgroundDisabled() {
    Bus bus;

    configureBackground(bus, 0x90);

    bus.Write(BGP, 0x03);

    PPU ppu(bus);
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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

    ppu.Step(252);

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
    ppu.Step(252);
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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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
    ppu.Step(252);

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

    ppu.Step(252);
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
    ppu.Step(252);

    // Y-flipping the whole 8x16 object makes screen line 0
    // read from the bottom tile.
    CHECK(pixel(ppu, 0, 0) == 2);
}

} // namespace

void run() {
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
