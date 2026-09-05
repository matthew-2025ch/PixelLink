#pragma once

namespace PixelLink::Test {

namespace GameBoy{
    namespace CPUTest {
        void run();
    }

    namespace InterruptTest {
        void run();
    }

    namespace CartridgeTest {
        void run();
    }

    namespace BusTest {
        void run();
    }

    namespace TimerTest {
        void run();
    }

    namespace TimerIntegrationTest {
        void run();
    }
} // namespace GameBoy

} // namespace PixelLink::Test