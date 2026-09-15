#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/GameBoy/Timer.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::BusTest {

namespace {

void testRomFallback() {
    Bus bus;

    bus.Write(0x0000, 0x12);
    bus.Write(0x0100, 0x34);
    bus.Write(0x7FFF, 0x56);

    CHECK(bus.Read(0x0000) == 0x12);
    CHECK(bus.Read(0x0100) == 0x34);
    CHECK(bus.Read(0x7FFF) == 0x56);
}

void testVRAM() {
    Bus bus;

    bus.Write(0x8000, 0x12);
    bus.Write(0x9000, 0x34);
    bus.Write(0x9FFF, 0x56);

    CHECK(bus.Read(0x8000) == 0x12);
    CHECK(bus.Read(0x9000) == 0x34);
    CHECK(bus.Read(0x9FFF) == 0x56);
}

void testWRAM() {
    Bus bus;

    bus.Write(0xC000, 0x11);
    bus.Write(0xC123, 0x22);
    bus.Write(0xDFFF, 0x33);

    CHECK(bus.Read(0xC000) == 0x11);
    CHECK(bus.Read(0xC123) == 0x22);
    CHECK(bus.Read(0xDFFF) == 0x33);
}

void testEchoRAM() {
    Bus bus;

    // WRAM -> Echo
    bus.Write(0xC123, 0x42);
    CHECK(bus.Read(0xE123) == 0x42);

    // Echo -> WRAM
    bus.Write(0xE456, 0x99);
    CHECK(bus.Read(0xC456) == 0x99);

    // Last mirrored address
    bus.Write(0xDDFF, 0xAB);
    CHECK(bus.Read(0xFDFF) == 0xAB);
}

void testOAM() {
    Bus bus;

    bus.Write(0xFE00, 0x12);
    bus.Write(0xFE50, 0x34);
    bus.Write(0xFE9F, 0x56);

    CHECK(bus.Read(0xFE00) == 0x12);
    CHECK(bus.Read(0xFE50) == 0x34);
    CHECK(bus.Read(0xFE9F) == 0x56);
}

void testUnusableMemory() {
    Bus bus;

    bus.Write(0xFEA0, 0x12);
    bus.Write(0xFEFF, 0x34);

    CHECK(bus.Read(0xFEA0) == 0xFF);
    CHECK(bus.Read(0xFEFF) == 0xFF);
}

void testIO() {
    Bus bus;

    // FF00 and FF04-FF07 are device-mapped registers, so use
    // ordinary I/O addresses for the generic I/O storage test.
    bus.Write(0xFF10, 0x12);
    bus.Write(0xFF0F, 0x1F);
    bus.Write(0xFF7F, 0x34);

    CHECK(bus.Read(0xFF10) == 0x12);
    CHECK(bus.Read(0xFF0F) == 0x1F);
    CHECK(bus.Read(0xFF7F) == 0x34);
}

void testDeviceAttachments() {
    Bus bus;
    Timer timer;
    Joypad joypad;

    // Detached device ranges read as open bus/high.
    CHECK(bus.Read(0xFF00) == 0xFF);
    CHECK(bus.Read(0xFF04) == 0xFF);

    bus.AttachTimer(timer);
    bus.Write(0xFF05, 0x42);
    CHECK(bus.Read(0xFF05) == 0x42);

    bus.AttachJoypad(joypad);
    bus.Write(0xFF00, 0x20);
    CHECK(bus.Read(0xFF00) == 0xEF);

    bus.DetachTimer(timer);
    bus.DetachJoypad(joypad);

    CHECK(bus.Read(0xFF00) == 0xFF);
    CHECK(bus.Read(0xFF04) == 0xFF);
}

void testHRAM() {
    Bus bus;

    bus.Write(0xFF80, 0x12);
    bus.Write(0xFFFC, 0x34);
    bus.Write(0xFFFE, 0x56);

    CHECK(bus.Read(0xFF80) == 0x12);
    CHECK(bus.Read(0xFFFC) == 0x34);
    CHECK(bus.Read(0xFFFE) == 0x56);
}

void testIE() {
    Bus bus;

    bus.Write(0xFFFF, 0x1F);
    CHECK(bus.Read(0xFFFF) == 0x1F);

    bus.Write(0xFFFF, 0x04);
    CHECK(bus.Read(0xFFFF) == 0x04);
}

void testRegionIsolation() {
    Bus bus;

    bus.Write(0x8000, 0x11);
    bus.Write(0xC000, 0x22);
    bus.Write(0xFE00, 0x33);
    bus.Write(0xFF80, 0x44);
    bus.Write(0xFFFF, 0x55);

    CHECK(bus.Read(0x8000) == 0x11);
    CHECK(bus.Read(0xC000) == 0x22);
    CHECK(bus.Read(0xFE00) == 0x33);
    CHECK(bus.Read(0xFF80) == 0x44);
    CHECK(bus.Read(0xFFFF) == 0x55);
}


void testPPUAccessRestrictions() {
    Bus bus;

    // LCD on.
    bus.Write(0xFF40, 0x80);

    // Preload data before the PPU starts restricting CPU access.
    bus.Write(0x8000, 0x12);
    bus.Write(0xFE00, 0x34);

    PPU ppu(bus);

    // Mode 2: CPU can access VRAM but not OAM.
    CHECK(ppu.GetMode() == PPU::Mode::OAMScan);
    CHECK(bus.Read(0x8000) == 0x12);
    CHECK(bus.Read(0xFE00) == 0xFF);

    bus.Write(0xFE00, 0x99);

    CHECK(
        bus.Read(0xFE00, BusAccess::PPU) ==
        0x34
    );

    // Mode 3: CPU cannot access VRAM or OAM.
    ppu.Step(80);

    CHECK(ppu.GetMode() == PPU::Mode::Drawing);
    CHECK(bus.Read(0x8000) == 0xFF);
    CHECK(bus.Read(0xFE00) == 0xFF);

    bus.Write(0x8000, 0x99);

    CHECK(
        bus.Read(0x8000, BusAccess::PPU) ==
        0x12
    );

    // Mode 0: both become accessible again.
    ppu.Step(172);

    CHECK(ppu.GetMode() == PPU::Mode::HBlank);
    CHECK(bus.Read(0x8000) == 0x12);
    CHECK(bus.Read(0xFE00) == 0x34);
}

void testOAMDMATransfer() {
    Bus bus;

    for (std::uint16_t i = 0; i < 0x00A0; ++i) {
        bus.Write(
            static_cast<std::uint16_t>(0xC000 + i),
            static_cast<std::uint8_t>(i ^ 0x5A)
        );
    }

    bus.Write(0xFF46, 0xC0);

    CHECK(bus.IsOAMDMAActive());
    CHECK(bus.GetOAMDMABytesTransferred() == 0);

    bus.Tick(4);

    CHECK(bus.IsOAMDMAActive());
    CHECK(bus.GetOAMDMABytesTransferred() == 1);

    CHECK(
        bus.Read(0xFE00, BusAccess::DMA) ==
        static_cast<std::uint8_t>(0x00 ^ 0x5A)
    );

    bus.Tick(636);

    CHECK(!bus.IsOAMDMAActive());
    CHECK(bus.GetOAMDMABytesTransferred() == 0x00A0);

    for (std::uint16_t i = 0; i < 0x00A0; ++i) {
        CHECK(
            bus.Read(
                static_cast<std::uint16_t>(0xFE00 + i)
            ) ==
            static_cast<std::uint8_t>(i ^ 0x5A)
        );
    }
}

void testOAMDMACPUTimingRestriction() {
    Bus bus;

    bus.Write(0xC000, 0x12);
    bus.Write(0xFF80, 0x34);

    bus.Write(0xFF46, 0xC0);

    CHECK(bus.IsOAMDMAActive());

    // During DMG OAM DMA, the CPU can access HRAM only.
    CHECK(bus.Read(0xC000) == 0xFF);
    CHECK(bus.Read(0xFF80) == 0x34);

    bus.Write(0xC000, 0x99);
    bus.Write(0xFF80, 0x56);

    CHECK(bus.Read(0xFF80) == 0x56);

    bus.Tick(640);

    CHECK(!bus.IsOAMDMAActive());

    // The blocked WRAM write must not have happened.
    CHECK(bus.Read(0xC000) == 0x12);
}

void testOAMDMARestart() {
    Bus bus;

    for (std::uint16_t i = 0; i < 0x00A0; ++i) {
        bus.Write(
            static_cast<std::uint16_t>(0xC000 + i),
            0x11
        );

        bus.Write(
            static_cast<std::uint16_t>(0xD000 + i),
            0x22
        );
    }

    bus.Write(0xFF46, 0xC0);
    bus.Tick(40);

    CHECK(bus.GetOAMDMABytesTransferred() == 10);

    // FF46 remains writable so an active transfer can restart.
    bus.Write(0xFF46, 0xD0);

    CHECK(bus.IsOAMDMAActive());
    CHECK(bus.GetOAMDMABytesTransferred() == 0);

    bus.Tick(640);

    CHECK(!bus.IsOAMDMAActive());

    for (std::uint16_t i = 0; i < 0x00A0; ++i) {
        CHECK(
            bus.Read(
                static_cast<std::uint16_t>(0xFE00 + i)
            ) == 0x22
        );
    }
}

} // namespace

void run() {
    Test::run("Bus / ROM fallback", testRomFallback);
    Test::run("Bus / VRAM", testVRAM);
    Test::run("Bus / WRAM", testWRAM);
    Test::run("Bus / Echo RAM", testEchoRAM);
    Test::run("Bus / OAM", testOAM);
    Test::run("Bus / unusable memory", testUnusableMemory);
    Test::run("Bus / I/O", testIO);
    Test::run("Bus / device attachments", testDeviceAttachments);
    Test::run("Bus / HRAM", testHRAM);
    Test::run("Bus / IE", testIE);
    Test::run("Bus / region isolation", testRegionIsolation);
    Test::run(
        "Bus / PPU VRAM-OAM restrictions",
        testPPUAccessRestrictions
    );
    Test::run("DMA / OAM transfer", testOAMDMATransfer);
    Test::run(
        "DMA / CPU HRAM-only restriction",
        testOAMDMACPUTimingRestriction
    );
    Test::run("DMA / restart", testOAMDMARestart);
}

} // namespace PixelLink::Test::GameBoy::BusTest
