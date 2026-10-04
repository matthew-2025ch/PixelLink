#include <array>
#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/GameBoy/Serial.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Test::GameBoy::SerialTest {
namespace {
using PixelLink::GameBoy::Serial;
constexpr auto SB = 0xFF01;
constexpr auto SC = 0xFF02;

void internalTransfer() {
    Serial serial;
    CHECK(serial.Read(SC) == 0x7E);
    serial.Write(SB, 0);
    serial.Write(SC, 0xFF); // DMG ignores fast-clock and unused control bits.
    serial.Tick(511);
    CHECK(serial.Read(SB) == 0);
    serial.Tick(1);
    CHECK(serial.Read(SB) == 1);
    serial.Tick(3583);
    CHECK(serial.Read(SB) == 0x7F);
    CHECK((serial.Read(SC) & 0x80) != 0);
    CHECK(!serial.ConsumeInterruptRequest());
    serial.Tick(1);
    CHECK(serial.Read(SB) == 0xFF);
    CHECK(serial.Read(SC) == 0x7F);
    CHECK(serial.ConsumeInterruptRequest());
    CHECK(!serial.ConsumeInterruptRequest());
    serial.Tick(4096);
    CHECK(!serial.ConsumeInterruptRequest());
}

void freeRunningClockAndAbort() {
    Serial serial;
    serial.Tick(500);
    serial.Write(SB, 0);
    serial.Write(SC, 0x81);
    serial.Tick(11);
    CHECK(serial.Read(SB) == 0);
    serial.Tick(1);
    CHECK(serial.Read(SB) == 1); // start does not reset the link clock
    serial.Write(SC, 0);
    serial.Tick(8192);
    CHECK(serial.Read(SB) == 1);
    CHECK(!serial.ConsumeInterruptRequest());
    serial.Write(SB, 0);
    serial.Write(SC, 0x81);
    serial.Tick(4096);
    CHECK(serial.Read(SB) == 0xFF);
    CHECK(serial.ConsumeInterruptRequest());
}

void externalClock() {
    Serial serial;
    serial.Write(SB, 0x3C);
    serial.Write(SC, 0x80);
    serial.Tick(8192);
    CHECK(serial.Read(SB) == 0x3C);
    CHECK((serial.Read(SC) & 0x80) != 0);
    for (int bit = 7; bit >= 0; --bit) {
        CHECK(serial.ClockExternalBit(((0xA5 >> bit) & 1) != 0)
            == (((0x3C >> bit) & 1) != 0));
        CHECK(serial.ConsumeInterruptRequest() == (bit == 0));
    }
    CHECK(serial.Read(SB) == 0xA5);
    CHECK(serial.Read(SC) == 0x7E);
    (void)serial.ClockExternalBit(false);
    CHECK(serial.Read(SB) == 0xA5);
}

void coreMappingAndInterrupt() {
    PixelLink::GameBoy::GameBoy gameBoy;
    auto& bus = gameBoy.GetBus();
    bus.Write(0xFF40, 0);
    bus.Write(0xFF0F, 0);
    bus.Write(SB, 0);
    bus.Write(SC, 0x81);
    // The DMG ABC post-boot link clock is at ABCC: the first falling edge
    // is 52 T-cycles away, then the other seven edges are 512 cycles apart.
    for (int i = 0; i < 908; ++i) {
        CHECK(gameBoy.Step() == 4);
    }
    CHECK(bus.Read(SB) == 0x7F);
    CHECK((bus.Read(0xFF0F) & 8) == 0);
    bus.Write(0xFF04, 0); // DIV reset must not move the independent serial clock.
    CHECK(gameBoy.Step() == 4);
    CHECK(bus.Read(SB) == 0xFF);
    CHECK((bus.Read(SC) & 0x80) == 0);
    CHECK((bus.Read(0xFF0F) & 8) != 0);
    bus.Write(0xFF0F, 0);
    CHECK(gameBoy.Step() == 4);
    CHECK((bus.Read(0xFF0F) & 8) == 0);
}
} // namespace

void run() {
    Test::run("Serial / internal bit timing and completion", internalTransfer);
    Test::run("Serial / free-running clock, abort and restart", freeRunningClockAndAbort);
    Test::run("Serial / external input and output", externalClock);
    Test::run("Serial / core mapping, DIV independence and IF", coreMappingAndInterrupt);
}
} // namespace PixelLink::Test::GameBoy::SerialTest
