#include <cstdint>

#include "Bus.hpp"
#include "CPU.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"
#include "TestUtils.hpp"

namespace CPUTest {

namespace {

constexpr uint8_t FLAG_Z = 0x80;
constexpr uint8_t FLAG_N = 0x40;
constexpr uint8_t FLAG_H = 0x20;
constexpr uint8_t FLAG_C = 0x10;

bool flagSet(
    const CPU& cpu,
    uint8_t flag
) {
    return (cpu.F & flag) != 0;
}

void testLoadAddFlags() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0x0F, // LD A,$0F
        0x06, 0x01, // LD B,$01
        0x80,       // ADD A,B
        0xC6, 0xF0, // ADD A,$F0
        0x76        // HALT
    });

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0x0F);

    CHECK(cpu.step() == 8);
    CHECK(cpu.B == 0x01);

    CHECK(cpu.step() == 4);
    CHECK(cpu.A == 0x10);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(!flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0x00);

    CHECK(flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 4);
    CHECK(cpu.halted);
}

void testIncDec() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x37,       // SCF
        0x06, 0x0F, // LD B,$0F
        0x04,       // INC B
        0x05,       // DEC B
        0x76
    });

    CHECK(cpu.step() == 4);
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 8);
    CHECK(cpu.B == 0x0F);

    CHECK(cpu.step() == 4);
    CHECK(cpu.B == 0x10);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 4);
    CHECK(cpu.B == 0x0F);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));
}

void testAdcSbc() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0xFF,
        0x37,
        0xCE, 0x00,
        0x3E, 0x00,
        0x37,
        0xDE, 0x00,
        0x76
    });

    cpu.step();
    cpu.step();

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0x00);

    CHECK(flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    cpu.step();
    cpu.step();

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0xFF);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));
}

void testSubAndCp() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0x10,
        0xD6, 0x01,
        0xFE, 0x0F,
        0x76
    });

    cpu.step();

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0x0F);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(!flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 8);

    CHECK(cpu.A == 0x0F);
    CHECK(flagSet(cpu, FLAG_Z));
    CHECK(flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(!flagSet(cpu, FLAG_C));
}

void testConditionalJump() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0x08,
        0xFE, 0x08,
        0x28, 0x02,
        0x3E, 0xFF,
        0xFE, 0x07,
        0x28, 0x02,
        0x3E, 0x42,
        0x76
    });

    cpu.step();
    cpu.step();

    CHECK(flagSet(cpu, FLAG_Z));

    CHECK(cpu.step() == 12);
    CHECK(cpu.PC == 0x0108);

    cpu.step();

    CHECK(!flagSet(cpu, FLAG_Z));

    CHECK(cpu.step() == 8);
    CHECK(cpu.PC == 0x010C);

    cpu.step();

    CHECK(cpu.A == 0x42);
}

void testCallReturn() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0xCD, 0x00, 0x02,
        0x3E, 0x42,
        0x76
    });

    Test::load(bus, 0x0200, {
        0x06, 0x99,
        0xC9
    });

    CHECK(cpu.step() == 24);

    CHECK(cpu.PC == 0x0200);
    CHECK(cpu.SP == 0xFFFC);

    CHECK(bus.read(0xFFFC) == 0x03);
    CHECK(bus.read(0xFFFD) == 0x01);

    CHECK(cpu.step() == 8);
    CHECK(cpu.B == 0x99);

    CHECK(cpu.step() == 16);

    CHECK(cpu.PC == 0x0103);
    CHECK(cpu.SP == 0xFFFE);

    CHECK(cpu.step() == 8);
    CHECK(cpu.A == 0x42);

    CHECK(cpu.step() == 4);
    CHECK(cpu.halted);
}

void testPushPop() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x01, 0x34, 0x12,
        0xC5,
        0x01, 0x00, 0x00,
        0xC1,
        0x76
    });

    CHECK(cpu.step() == 12);

    CHECK(cpu.B == 0x12);
    CHECK(cpu.C == 0x34);

    CHECK(cpu.step() == 16);

    CHECK(cpu.SP == 0xFFFC);
    CHECK(bus.read(0xFFFC) == 0x34);
    CHECK(bus.read(0xFFFD) == 0x12);

    CHECK(cpu.step() == 12);

    CHECK(cpu.B == 0x00);
    CHECK(cpu.C == 0x00);

    CHECK(cpu.step() == 12);

    CHECK(cpu.B == 0x12);
    CHECK(cpu.C == 0x34);
    CHECK(cpu.SP == 0xFFFE);
}

void testPopAFMask() {
    Bus bus;
    CPU cpu(bus);

    bus.write(0xC000, 0xFF);
    bus.write(0xC001, 0x12);

    Test::load(bus, 0x0100, {
        0x31, 0x00, 0xC0,
        0xF1,
        0xF5,
        0x76
    });

    CHECK(cpu.step() == 12);
    CHECK(cpu.SP == 0xC000);

    CHECK(cpu.step() == 12);

    CHECK(cpu.A == 0x12);
    CHECK(cpu.F == 0xF0);
    CHECK(cpu.SP == 0xC002);

    CHECK(cpu.step() == 16);

    CHECK(cpu.SP == 0xC000);
    CHECK(bus.read(0xC000) == 0xF0);
    CHECK(bus.read(0xC001) == 0x12);
}

void testMemoryAndCB() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x21, 0x00, 0xC0,
        0x36, 0x80,
        0xCB, 0x06,
        0xCB, 0x46,
        0xCB, 0x86,
        0xCB, 0xC6,
        0x7E,
        0x76
    });

    CHECK(cpu.step() == 12);

    CHECK(cpu.H == 0xC0);
    CHECK(cpu.L == 0x00);

    CHECK(cpu.step() == 12);
    CHECK(bus.read(0xC000) == 0x80);

    CHECK(cpu.step() == 16);

    CHECK(bus.read(0xC000) == 0x01);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 12);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 16);
    CHECK(bus.read(0xC000) == 0x00);
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 16);
    CHECK(bus.read(0xC000) == 0x01);

    cpu.step();
    CHECK(cpu.A == 0x01);
}

void testAddHL() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0x00,
        0xFE, 0x00,
        0x21, 0xFF, 0x0F,
        0x01, 0x01, 0x00,
        0x09,
        0x01, 0x00, 0xF0,
        0x09,
        0x76
    });

    cpu.step();
    cpu.step();

    CHECK(flagSet(cpu, FLAG_Z));

    cpu.step();
    cpu.step();

    CHECK(cpu.step() == 8);

    CHECK(cpu.H == 0x10);
    CHECK(cpu.L == 0x00);

    CHECK(flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(!flagSet(cpu, FLAG_C));

    cpu.step();

    CHECK(cpu.step() == 8);

    CHECK(cpu.H == 0x00);
    CHECK(cpu.L == 0x00);

    CHECK(flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));
}

void testSignedSP() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x31, 0xF8, 0xFF,
        0xE8, 0x08,
        0x31, 0x08, 0x00,
        0xF8, 0xF8,
        0x76
    });

    CHECK(cpu.step() == 12);
    CHECK(cpu.SP == 0xFFF8);

    CHECK(cpu.step() == 16);
    CHECK(cpu.SP == 0x0000);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));

    CHECK(cpu.step() == 12);
    CHECK(cpu.SP == 0x0008);

    CHECK(cpu.step() == 12);

    CHECK(cpu.H == 0x00);
    CHECK(cpu.L == 0x00);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));
}

void testDAA() {
    Bus bus;
    CPU cpu(bus);

    Test::load(bus, 0x0100, {
        0x3E, 0x15,
        0xC6, 0x27,
        0x27,
        0x3E, 0x88,
        0xC6, 0x99,
        0x27,
        0x76
    });

    cpu.step();
    cpu.step();

    CHECK(cpu.A == 0x3C);

    CHECK(cpu.step() == 4);
    CHECK(cpu.A == 0x42);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(!flagSet(cpu, FLAG_C));

    cpu.step();
    cpu.step();

    CHECK(cpu.step() == 4);
    CHECK(cpu.A == 0x87);

    CHECK(!flagSet(cpu, FLAG_Z));
    CHECK(!flagSet(cpu, FLAG_N));
    CHECK(!flagSet(cpu, FLAG_H));
    CHECK(flagSet(cpu, FLAG_C));
}

} // namespace

void run() {
    Test::run("CPU / LD / ADD / flags", testLoadAddFlags);
    Test::run("CPU / INC / DEC", testIncDec);
    Test::run("CPU / ADC / SBC", testAdcSbc);
    Test::run("CPU / SUB / CP", testSubAndCp);
    Test::run("CPU / conditional JR", testConditionalJump);
    Test::run("CPU / CALL / RET", testCallReturn);
    Test::run("CPU / PUSH / POP", testPushPop);
    Test::run("CPU / POP AF mask", testPopAFMask);
    Test::run("CPU / CB / memory", testMemoryAndCB);
    Test::run("CPU / ADD HL", testAddHL);
    Test::run("CPU / signed SP", testSignedSP);
    Test::run("CPU / DAA", testDAA);
}

} // namespace CPUTest
