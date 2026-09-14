#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::PPUTest {

namespace {

constexpr uint16_t LCDC = 0xFF40;
constexpr uint16_t STAT = 0xFF41;
constexpr uint16_t LY = 0xFF44;
constexpr uint16_t LYC = 0xFF45;
constexpr uint16_t IF = 0xFF0F;

void enableLCD(Bus& bus) {
    bus.Write(
        LCDC,
        static_cast<uint8_t>(bus.Read(LCDC) | 0x80)
    );
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
    CHECK((bus.Read(IF) & 0x01) != 0);
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
    CHECK((bus.Read(STAT) & 0x04) == 0);

    ppu.Step(456);

    CHECK(ppu.GetLY() == 1);
    CHECK(bus.Read(LY) == 1);
    CHECK((bus.Read(STAT) & 0x04) != 0);
}

void testLCDDisabledState() {
    Bus bus;
    bus.Write(LCDC, 0x00);

    PPU ppu(bus);

    ppu.Step(1000);

    CHECK(ppu.GetLY() == 0);
    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(ppu.GetLineDot() == 0);
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

} // namespace

void run() {
    Test::run("PPU / visible scanline timing", testVisibleScanlineTiming);
    Test::run("PPU / VBlank entry", testVBlankEntry);
    Test::run("PPU / frame length", testFrameLength);
    Test::run("PPU / LY / LYC coincidence", testCoincidenceFlag);
    Test::run("PPU / LCD disabled state", testLCDDisabledState);
    Test::run("PPU / LCD enable", testLCDEnableAfterDisabled);
}

} // namespace PixelLink::Test::GameBoy::PPUTest
