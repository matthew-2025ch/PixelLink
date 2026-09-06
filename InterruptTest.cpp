#include <cstdint>

#include "Bus.hpp"
#include "CPU.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"
#include "TestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::InterruptTest {

namespace {

constexpr uint16_t IF = 0xFF0F;
constexpr uint16_t IE = 0xFFFF;

void testEIDelay() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0xFB, // EI
        0x00, // NOP
        0x00  // should NOT execute before interrupt
    });

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.PC == 0x0101);
    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.PC == 0x0102);
    CHECK(cpu.ime == true);

    CHECK(cpu.Step() == 20);
    CHECK(cpu.PC == 0x0040);
    CHECK(cpu.ime == false);
}

void testVBlankInterrupt() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0xFB, // EI
        0x00  // NOP
    });

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    cpu.Step();
    cpu.Step();

    CHECK(cpu.PC == 0x0102);
    CHECK(cpu.SP == 0xFFFE);

    CHECK(cpu.Step() == 20);

    CHECK(cpu.PC == 0x0040);

    CHECK(cpu.SP == 0xFFFC);
    CHECK(bus.Read(0xFFFC) == 0x02);
    CHECK(bus.Read(0xFFFD) == 0x01);

    CHECK((bus.Read(IF) & 0x01) == 0);
    CHECK(cpu.ime == false);
}

void testInterruptPriority() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0xFB,
        0x00
    });

    bus.Write(IE, 0x1F);

    bus.Write(
        IF,
        static_cast<uint8_t>(
            (1u << 0)
            | (1u << 2)
            | (1u << 4)
        )
    );

    cpu.Step();
    cpu.Step();

    CHECK(cpu.Step() == 20);

    CHECK(cpu.PC == 0x0040);
    CHECK((bus.Read(IF) & 0x01) == 0);
    CHECK((bus.Read(IF) & 0x04) != 0);
    CHECK((bus.Read(IF) & 0x10) != 0);
}

void testInterruptWaitsWhenIMEDisabled() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0x00,
        0x00
    });

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.PC == 0x0101);
    CHECK((bus.Read(IF) & 0x01) != 0);
}

void testHaltWakeWithoutIME() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0x76,
        0x00
    });

    CHECK(cpu.Step() == 4);
    CHECK(cpu.halted);
    CHECK(cpu.PC == 0x0101);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.halted);
    CHECK(cpu.PC == 0x0101);

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    CHECK(cpu.Step() == 4);

    CHECK(!cpu.halted);
    CHECK(cpu.PC == 0x0102);
    CHECK((bus.Read(IF) & 0x01) != 0);
}

void testDICancelsEI() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0xFB,
        0xF3,
        0x00
    });

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.PC == 0x0103);
    CHECK(cpu.ime == false);
}

void testRETI() {
    Bus bus;
    CPU cpu(bus);

    Test::Load(bus, 0x0100, {
        0xFB,
        0x00
    });

    Test::Load(bus, 0x0040, {
        0xD9
    });

    bus.Write(IE, 0x01);
    bus.Write(IF, 0x01);

    cpu.Step();
    cpu.Step();

    CHECK(cpu.Step() == 20);

    CHECK(cpu.PC == 0x0040);
    CHECK(cpu.ime == false);

    CHECK(cpu.Step() == 16);

    CHECK(cpu.PC == 0x0102);
    CHECK(cpu.SP == 0xFFFE);
    CHECK(cpu.ime == true);
}

void testInterruptVectors() {
    struct TestCase {
        uint8_t bit;
        uint16_t vector;
    };

    constexpr TestCase cases[] = {
        { 0, 0x0040 },
        { 1, 0x0048 },
        { 2, 0x0050 },
        { 3, 0x0058 },
        { 4, 0x0060 }
    };

    for (const auto& test : cases) {
        Bus bus;
        CPU cpu(bus);

        Test::Load(bus, 0x0100, {
            0xFB,
            0x00
        });

        const uint8_t mask =
            static_cast<uint8_t>(
                1u << test.bit
            );

        bus.Write(IE, mask);
        bus.Write(IF, mask);

        cpu.Step();
        cpu.Step();

        CHECK(cpu.Step() == 20);
        CHECK(cpu.PC == test.vector);
        CHECK((bus.Read(IF) & mask) == 0);
    }
}

} // namespace

void run() {
    Test::run("Interrupt / EI delay", testEIDelay);
    Test::run("Interrupt / VBlank", testVBlankInterrupt);
    Test::run("Interrupt / priority", testInterruptPriority);
    Test::run("Interrupt / IME disabled", testInterruptWaitsWhenIMEDisabled);
    Test::run("Interrupt / HALT wake-up", testHaltWakeWithoutIME);
    Test::run("Interrupt / DI cancels EI", testDICancelsEI);
    Test::run("Interrupt / RETI", testRETI);
    Test::run("Interrupt / vectors", testInterruptVectors);
}

} // namespace PixelLink::Test::InterruptTest