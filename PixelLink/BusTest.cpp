#include <cstdint>

#include "Bus.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"

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

    bus.Write(0xFF00, 0x12);
    bus.Write(0xFF0F, 0x1F);
    bus.Write(0xFF7F, 0x34);

    CHECK(bus.Read(0xFF00) == 0x12);
    CHECK(bus.Read(0xFF0F) == 0x1F);
    CHECK(bus.Read(0xFF7F) == 0x34);
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

} // namespace

void run() {
    Test::run("Bus / ROM fallback", testRomFallback);
    Test::run("Bus / VRAM", testVRAM);
    Test::run("Bus / WRAM", testWRAM);
    Test::run("Bus / Echo RAM", testEchoRAM);
    Test::run("Bus / OAM", testOAM);
    Test::run("Bus / unusable memory", testUnusableMemory);
    Test::run("Bus / I/O", testIO);
    Test::run("Bus / HRAM", testHRAM);
    Test::run("Bus / IE", testIE);
    Test::run("Bus / region isolation", testRegionIsolation);
}

} // namespace PixelLink::Test::BusTest
