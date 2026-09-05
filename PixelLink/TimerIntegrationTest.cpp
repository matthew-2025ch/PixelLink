#include <cstdint>

#include "Bus.hpp"
#include "CPU.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"
#include "TestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::TimerIntegrationTest {

namespace {

constexpr uint16_t DIV  = 0xFF04;
constexpr uint16_t TIMA = 0xFF05;
constexpr uint16_t TMA  = 0xFF06;
constexpr uint16_t TAC  = 0xFF07;
constexpr uint16_t IF   = 0xFF0F;
constexpr uint16_t IE   = 0xFFFF;

constexpr uint8_t TIMER_INTERRUPT = 1u << 2;

void testBusTimerRegisterMapping() {
    Bus bus;

    bus.Write(TIMA, 0x12);
    bus.Write(TMA, 0x34);
    bus.Write(TAC, 0x05);

    CHECK(bus.Read(TIMA) == 0x12);
    CHECK(bus.Read(TMA) == 0x34);
    CHECK(bus.Read(TAC) == 0xFD);

    bus.Tick(256);
    CHECK(bus.Read(DIV) == 0x01);

    bus.Write(DIV, 0xAB);
    CHECK(bus.Read(DIV) == 0x00);
}

void testCpuCyclesAdvanceTimer() {
    Bus bus;
    CPU cpu(bus);

    bus.Write(TIMA, 0x00);
    bus.Write(TAC, 0x05);

    Test::Load(bus, 0x0100, {
        0x00, // NOP - 4 T-cycles
        0x00, // NOP - 4 T-cycles
        0x00, // NOP - 4 T-cycles
        0x00  // NOP - 4 T-cycles
    });

    CHECK(bus.Read(TIMA) == 0x00);

    CHECK(cpu.Step() == 4);
    CHECK(cpu.Step() == 4);
    CHECK(cpu.Step() == 4);

    CHECK(bus.Read(TIMA) == 0x00);

    CHECK(cpu.Step() == 4);
    CHECK(bus.Read(TIMA) == 0x01);
}

void testHaltStillAdvancesTimer() {
    Bus bus;
    CPU cpu(bus);

    bus.Write(TIMA, 0x00);
    bus.Write(TAC, 0x05);

    Test::Load(bus, 0x0100, {
        0x76 // HALT - 4 T-cycles
    });

    CHECK(cpu.Step() == 4);
    CHECK(cpu.halted);

    // A halted CPU still consumes clock cycles.
    CHECK(cpu.Step() == 4);
    CHECK(cpu.Step() == 4);
    CHECK(cpu.Step() == 4);

    CHECK(bus.Read(TIMA) == 0x01);
}

void testTimerRequestsInterruptThroughBus() {
    Bus bus;

    bus.Write(IF, 0x00);
    bus.Write(TMA, 0x42);
    bus.Write(TIMA, 0xFF);
    bus.Write(TAC, 0x05);

    // 16 T-cycles produce the TIMA overflow.
    bus.Tick(16);

    CHECK(bus.Read(TIMA) == 0x00);
    CHECK((bus.Read(IF) & TIMER_INTERRUPT) == 0);

    // The reload and interrupt request happen 4 T-cycles later.
    bus.Tick(4);

    CHECK(bus.Read(TIMA) == 0x42);
    CHECK((bus.Read(IF) & TIMER_INTERRUPT) != 0);
}

void testCpuServicesTimerInterrupt() {
    Bus bus;
    CPU cpu(bus);

    bus.Write(IF, 0x00);
    bus.Write(IE, TIMER_INTERRUPT);

    bus.Write(TMA, 0x42);
    bus.Write(TIMA, 0xFF);
    bus.Write(TAC, 0x05);

    Test::Load(bus, 0x0100, {
        0xFB, // EI
        0x00, // NOP
        0x00, // NOP
        0x00, // NOP -> TIMA overflows at 16 T-cycles total
        0x00  // NOP -> reload + IF request at 20 T-cycles total
    });

    CHECK(cpu.Step() == 4); // EI
    CHECK(cpu.Step() == 4); // IME becomes enabled after this instruction
    CHECK(cpu.Step() == 4);
    CHECK(cpu.Step() == 4); // TIMA overflows

    CHECK(bus.Read(TIMA) == 0x00);
    CHECK((bus.Read(IF) & TIMER_INTERRUPT) == 0);

    CHECK(cpu.Step() == 4); // TIMA reloads and requests interrupt

    CHECK(bus.Read(TIMA) == 0x42);
    CHECK((bus.Read(IF) & TIMER_INTERRUPT) != 0);
    CHECK(cpu.PC == 0x0105);

    CHECK(cpu.Step() == 20); // Service Timer interrupt

    CHECK(cpu.PC == 0x0050);
    CHECK(cpu.SP == 0xFFFC);
    CHECK(bus.Read(0xFFFC) == 0x05);
    CHECK(bus.Read(0xFFFD) == 0x01);
    CHECK((bus.Read(IF) & TIMER_INTERRUPT) == 0);
}

} // namespace

void run() {
    Test::run(
        "Timer integration / Bus register mapping",
        testBusTimerRegisterMapping
    );

    Test::run(
        "Timer integration / CPU cycles",
        testCpuCyclesAdvanceTimer
    );

    Test::run(
        "Timer integration / HALT cycles",
        testHaltStillAdvancesTimer
    );

    Test::run(
        "Timer integration / interrupt request",
        testTimerRequestsInterruptThroughBus
    );

    Test::run(
        "Timer integration / CPU interrupt service",
        testCpuServicesTimerInterrupt
    );
}

} // namespace PixelLink::Test::GameBoy::TimerIntegrationTest
