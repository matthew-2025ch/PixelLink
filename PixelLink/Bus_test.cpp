#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "Bus.hpp"

namespace {

#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            throw std::runtime_error(                                 \
                std::format(                                         \
                    "CHECK failed: {} at {}:{}",                      \
                    #expr,                                            \
                    __FILE__,                                         \
                    __LINE__                                          \
                )                                                     \
            );                                                        \
        }                                                             \
    } while (false)

    template <typename Func>
    void runTest(
        std::string_view name,
        Func test
    ) {
        try {
            test();

            std::cout
                << "[PASS] "
                << name
                << '\n';
        }
        catch (const std::exception& e) {
            std::cerr
                << "[FAIL] "
                << name
                << '\n'
                << "       "
                << e.what()
                << '\n';

            throw;
        }
    }

    void testTestRom() {
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

int main() {
    try {
        runTest(
            "test ROM fallback",
            testTestRom
        );

        runTest(
            "VRAM",
            testVRAM
        );

        runTest(
            "WRAM",
            testWRAM
        );

        runTest(
            "Echo RAM",
            testEchoRAM
        );

        runTest(
            "OAM",
            testOAM
        );

        runTest(
            "unusable memory",
            testUnusableMemory
        );

        runTest(
            "I/O",
            testIO
        );

        runTest(
            "HRAM",
            testHRAM
        );

        runTest(
            "IE",
            testIE
        );

        runTest(
            "region isolation",
            testRegionIsolation
        );
    }
    catch (...) {
        std::cerr
            << "\nBus tests FAILED.\n";

        return 1;
    }

    std::cout
        << "\nAll Bus tests passed.\n";

    return 0;
}