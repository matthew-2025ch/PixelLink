#include <cstdint>

#include "Bus.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"

namespace BusTest {

namespace {

void testRomFallback() {
    Bus bus;

    bus.write(0x0000, 0x12);
    bus.write(0x0100, 0x34);
    bus.write(0x7FFF, 0x56);

    CHECK(bus.read(0x0000) == 0x12);
    CHECK(bus.read(0x0100) == 0x34);
    CHECK(bus.read(0x7FFF) == 0x56);
}

void testVRAM() {
    Bus bus;

    bus.write(0x8000, 0x12);
    bus.write(0x9000, 0x34);
    bus.write(0x9FFF, 0x56);

    CHECK(bus.read(0x8000) == 0x12);
    CHECK(bus.read(0x9000) == 0x34);
    CHECK(bus.read(0x9FFF) == 0x56);
}

void testWRAM() {
    Bus bus;

    bus.write(0xC000, 0x11);
    bus.write(0xC123, 0x22);
    bus.write(0xDFFF, 0x33);

    CHECK(bus.read(0xC000) == 0x11);
    CHECK(bus.read(0xC123) == 0x22);
    CHECK(bus.read(0xDFFF) == 0x33);
}

void testEchoRAM() {
    Bus bus;

    // WRAM -> Echo
    bus.write(0xC123, 0x42);
    CHECK(bus.read(0xE123) == 0x42);

    // Echo -> WRAM
    bus.write(0xE456, 0x99);
    CHECK(bus.read(0xC456) == 0x99);

    // Last mirrored address
    bus.write(0xDDFF, 0xAB);
    CHECK(bus.read(0xFDFF) == 0xAB);
}

void testOAM() {
    Bus bus;

    bus.write(0xFE00, 0x12);
    bus.write(0xFE50, 0x34);
    bus.write(0xFE9F, 0x56);

    CHECK(bus.read(0xFE00) == 0x12);
    CHECK(bus.read(0xFE50) == 0x34);
    CHECK(bus.read(0xFE9F) == 0x56);
}

void testUnusableMemory() {
    Bus bus;

    bus.write(0xFEA0, 0x12);
    bus.write(0xFEFF, 0x34);

    CHECK(bus.read(0xFEA0) == 0xFF);
    CHECK(bus.read(0xFEFF) == 0xFF);
}

void testIO() {
    Bus bus;

    bus.write(0xFF00, 0x12);
    bus.write(0xFF0F, 0x1F);
    bus.write(0xFF7F, 0x34);

    CHECK(bus.read(0xFF00) == 0x12);
    CHECK(bus.read(0xFF0F) == 0x1F);
    CHECK(bus.read(0xFF7F) == 0x34);
}

void testHRAM() {
    Bus bus;

    bus.write(0xFF80, 0x12);
    bus.write(0xFFFC, 0x34);
    bus.write(0xFFFE, 0x56);

    CHECK(bus.read(0xFF80) == 0x12);
    CHECK(bus.read(0xFFFC) == 0x34);
    CHECK(bus.read(0xFFFE) == 0x56);
}

void testIE() {
    Bus bus;

    bus.write(0xFFFF, 0x1F);
    CHECK(bus.read(0xFFFF) == 0x1F);

    bus.write(0xFFFF, 0x04);
    CHECK(bus.read(0xFFFF) == 0x04);
}

void testRegionIsolation() {
    Bus bus;

    bus.write(0x8000, 0x11);
    bus.write(0xC000, 0x22);
    bus.write(0xFE00, 0x33);
    bus.write(0xFF80, 0x44);
    bus.write(0xFFFF, 0x55);

    CHECK(bus.read(0x8000) == 0x11);
    CHECK(bus.read(0xC000) == 0x22);
    CHECK(bus.read(0xFE00) == 0x33);
    CHECK(bus.read(0xFF80) == 0x44);
    CHECK(bus.read(0xFFFF) == 0x55);
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

} // namespace BusTest
