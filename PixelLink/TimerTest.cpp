#include <cstdint>

#include "TestFramework.hpp"
#include "TestSuites.hpp"
#include "Timer.hpp"

namespace TimerTest {

    namespace {

        constexpr uint16_t DIV = 0xFF04;
        constexpr uint16_t TIMA = 0xFF05;
        constexpr uint16_t TMA = 0xFF06;
        constexpr uint16_t TAC = 0xFF07;

        void testDivIncrement() {
            Timer timer;

            CHECK(timer.read(DIV) == 0x00);

            timer.tick(255);
            CHECK(timer.read(DIV) == 0x00);

            timer.tick(1);
            CHECK(timer.read(DIV) == 0x01);

            timer.tick(256);
            CHECK(timer.read(DIV) == 0x02);
        }

        void testDivReset() {
            Timer timer;

            timer.tick(256);

            CHECK(timer.read(DIV) == 0x01);

            // The written value is ignored.
            // Writing to DIV resets the internal divider counter.
            timer.write(DIV, 0xAB);

            CHECK(timer.read(DIV) == 0x00);
        }

        void testTimerDisabled() {
            Timer timer;

            timer.write(TIMA, 0x10);

            timer.tick(4096);

            CHECK(timer.read(TIMA) == 0x10);
        }

        void testFrequency4096() {
            Timer timer;

            // Enable timer, clock select 00.
            timer.write(TAC, 0x04);

            timer.tick(1023);
            CHECK(timer.read(TIMA) == 0x00);

            timer.tick(1);
            CHECK(timer.read(TIMA) == 0x01);

            timer.tick(1024);
            CHECK(timer.read(TIMA) == 0x02);
        }

        void testFrequency262144() {
            Timer timer;

            // Enable timer, clock select 01.
            timer.write(TAC, 0x05);

            timer.tick(15);
            CHECK(timer.read(TIMA) == 0x00);

            timer.tick(1);
            CHECK(timer.read(TIMA) == 0x01);

            timer.tick(16);
            CHECK(timer.read(TIMA) == 0x02);
        }

        void testFrequency65536() {
            Timer timer;

            // Enable timer, clock select 10.
            timer.write(TAC, 0x06);

            timer.tick(63);
            CHECK(timer.read(TIMA) == 0x00);

            timer.tick(1);
            CHECK(timer.read(TIMA) == 0x01);

            timer.tick(64);
            CHECK(timer.read(TIMA) == 0x02);
        }

        void testFrequency16384() {
            Timer timer;

            // Enable timer, clock select 11.
            timer.write(TAC, 0x07);

            timer.tick(255);
            CHECK(timer.read(TIMA) == 0x00);

            timer.tick(1);
            CHECK(timer.read(TIMA) == 0x01);

            timer.tick(256);
            CHECK(timer.read(TIMA) == 0x02);
        }

        void testTimaOverflow() {
            Timer timer;

            timer.write(TMA, 0x42);
            timer.write(TIMA, 0xFF);

            // Enable timer at 262144 Hz.
            timer.write(TAC, 0x05);

            // TIMA overflows after 16 T-cycles.
            timer.tick(16);

            CHECK(timer.read(TIMA) == 0x00);
            CHECK(!timer.consumeInterruptRequest());

            // Reload happens 4 T-cycles later.
            timer.tick(4);

            CHECK(timer.read(TIMA) == 0x42);
            CHECK(timer.consumeInterruptRequest());
        }

        void testInterruptRequestConsumed() {
            Timer timer;

            timer.write(TMA, 0x80);
            timer.write(TIMA, 0xFF);
            timer.write(TAC, 0x05);

            timer.tick(20);

            CHECK(timer.read(TIMA) == 0x80);

            CHECK(timer.consumeInterruptRequest());
            CHECK(!timer.consumeInterruptRequest());
        }

        void testDivWriteFallingEdge() {
            Timer timer;

            // TAC = 101:
            // enabled, using system counter bit 3.
            timer.write(TAC, 0x05);

            // After 8 T-cycles bit 3 is high.
            timer.tick(8);

            CHECK(timer.read(TIMA) == 0x00);

            // Resetting DIV changes the timer signal from 1 to 0,
            // producing a falling edge.
            timer.write(DIV, 0x00);

            CHECK(timer.read(TIMA) == 0x01);
        }

        void testTacReadMask() {
            Timer timer;

            timer.write(TAC, 0x05);

            // Upper unused bits read as 1.
            CHECK(timer.read(TAC) == 0xFD);
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

        Test::run("Timer / TAC read mask", testTacReadMask);
    }

} // namespace TimerTest