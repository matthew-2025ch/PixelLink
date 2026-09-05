#include <cstdint>

#include "TestFramework.hpp"
#include "TestSuites.hpp"
#include "Timer.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::TimerTest {

    namespace {

        constexpr uint16_t DIV = 0xFF04;
        constexpr uint16_t TIMA = 0xFF05;
        constexpr uint16_t TMA = 0xFF06;
        constexpr uint16_t TAC = 0xFF07;

        void testDivIncrement() {
            Timer timer;

            CHECK(timer.Read(DIV) == 0x00);

            timer.Tick(255);
            CHECK(timer.Read(DIV) == 0x00);

            timer.Tick(1);
            CHECK(timer.Read(DIV) == 0x01);

            timer.Tick(256);
            CHECK(timer.Read(DIV) == 0x02);
        }

        void testDivReset() {
            Timer timer;

            timer.Tick(256);

            CHECK(timer.Read(DIV) == 0x01);

            // The written value is ignored.
            // Writing to DIV resets the internal divider counter.
            timer.Write(DIV, 0xAB);

            CHECK(timer.Read(DIV) == 0x00);
        }

        void testTimerDisabled() {
            Timer timer;

            timer.Write(TIMA, 0x10);

            timer.Tick(4096);

            CHECK(timer.Read(TIMA) == 0x10);
        }

        void testFrequency4096() {
            Timer timer;

            // Enable timer, clock select 00.
            timer.Write(TAC, 0x04);

            timer.Tick(1023);
            CHECK(timer.Read(TIMA) == 0x00);

            timer.Tick(1);
            CHECK(timer.Read(TIMA) == 0x01);

            timer.Tick(1024);
            CHECK(timer.Read(TIMA) == 0x02);
        }

        void testFrequency262144() {
            Timer timer;

            // Enable timer, clock select 01.
            timer.Write(TAC, 0x05);

            timer.Tick(15);
            CHECK(timer.Read(TIMA) == 0x00);

            timer.Tick(1);
            CHECK(timer.Read(TIMA) == 0x01);

            timer.Tick(16);
            CHECK(timer.Read(TIMA) == 0x02);
        }

        void testFrequency65536() {
            Timer timer;

            // Enable timer, clock select 10.
            timer.Write(TAC, 0x06);

            timer.Tick(63);
            CHECK(timer.Read(TIMA) == 0x00);

            timer.Tick(1);
            CHECK(timer.Read(TIMA) == 0x01);

            timer.Tick(64);
            CHECK(timer.Read(TIMA) == 0x02);
        }

        void testFrequency16384() {
            Timer timer;

            // Enable timer, clock select 11.
            timer.Write(TAC, 0x07);

            timer.Tick(255);
            CHECK(timer.Read(TIMA) == 0x00);

            timer.Tick(1);
            CHECK(timer.Read(TIMA) == 0x01);

            timer.Tick(256);
            CHECK(timer.Read(TIMA) == 0x02);
        }

        void testTimaOverflow() {
            Timer timer;

            timer.Write(TMA, 0x42);
            timer.Write(TIMA, 0xFF);

            // Enable timer at 262144 Hz.
            timer.Write(TAC, 0x05);

            // TIMA overflows after 16 T-cycles.
            timer.Tick(16);

            CHECK(timer.Read(TIMA) == 0x00);
            CHECK(!timer.ConsumeInterruptRequest());

            // Reload happens 4 T-cycles later.
            timer.Tick(4);

            CHECK(timer.Read(TIMA) == 0x42);
            CHECK(timer.ConsumeInterruptRequest());
        }

        void testInterruptRequestConsumed() {
            Timer timer;

            timer.Write(TMA, 0x80);
            timer.Write(TIMA, 0xFF);
            timer.Write(TAC, 0x05);

            timer.Tick(20);

            CHECK(timer.Read(TIMA) == 0x80);

            CHECK(timer.ConsumeInterruptRequest());
            CHECK(!timer.ConsumeInterruptRequest());
        }

        void testDivWriteFallingEdge() {
            Timer timer;

            // TAC = 101:
            // enabled, using system counter bit 3.
            timer.Write(TAC, 0x05);

            // After 8 T-cycles bit 3 is high.
            timer.Tick(8);

            CHECK(timer.Read(TIMA) == 0x00);

            // Resetting DIV changes the timer signal from 1 to 0,
            // producing a falling edge.
            timer.Write(DIV, 0x00);

            CHECK(timer.Read(TIMA) == 0x01);
        }

        void testTacReadMask() {
            Timer timer;

            timer.Write(TAC, 0x05);

            // Upper unused bits Read as 1.
            CHECK(timer.Read(TAC) == 0xFD);
        }

    } // namespace

    void run() {
        Test::run("Timer / DIV increment", testDivIncrement);
        Test::run("Timer / DIV reset", testDivReset);
        Test::run("Timer / disabled", testTimerDisabled);

        Test::run("Timer / 4096 Hz", testFrequency4096);
        Test::run("Timer / 262144 Hz", testFrequency262144);
        Test::run("Timer / 65536 Hz", testFrequency65536);
        Test::run("Timer / 16384 Hz", testFrequency16384);

        Test::run("Timer / TIMA overflow", testTimaOverflow);
        Test::run(
            "Timer / interrupt request",
            testInterruptRequestConsumed
        );

        Test::run(
            "Timer / DIV falling edge",
            testDivWriteFallingEdge
        );

        Test::run("Timer / TAC Read mask", testTacReadMask);
    }

} // namespace PixelLink::Test::TimerTest