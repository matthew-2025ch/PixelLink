#pragma once

namespace PixelLink::Test {

namespace GameBoy{

    namespace BusTest {
        void run();
    }

    namespace CartridgeTest {
        void run();
    }

    namespace CPUTest {
        void run();
    }

    namespace InterruptTest {
        void run();
    }

    namespace JoypadTest {
        void run();
    }

    namespace MapperFactoryTest {
        void run();
    }

    namespace MBC1Test {
        void run();
    }

    namespace MBC3Test {
        void run();
    }

    namespace MBC5Test {
        void run();
    }

    namespace PPUTest {
        void run();
    }

    namespace RTCTest {
        void run();
    }

    namespace TimerIntegrationTest {
        void run();
    }

    namespace TimerTest {
        void run();
    }
} // namespace GameBoy

namespace Frontend {
    namespace EmulatorTest {
        void run();
    }
} // namespace Frontend

} // namespace PixelLink::Test