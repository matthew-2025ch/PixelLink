#pragma once

namespace PixelLink::Test {

namespace GameBoy{

    namespace APUTest {
        void run();
    }

    namespace APUAccuracyTest {
        void run();
    }

    namespace BusTest {
        void run();
    }

    namespace CartridgeTest {
        void run();
    }

    namespace CPUTest {
        void run();
    }

    namespace CoreTimingTest {
        void run();
    }

    namespace HardwareAccuracy {
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

    namespace SaveTest {
        void run();
    }

    namespace SerialTest {
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
    namespace AudioTest {
        void run();
    }

    namespace EmulatorTest {
        void run();
    }
} // namespace Frontend

} // namespace PixelLink::Test

namespace PixelLink::Test::Desktop {

namespace AudioTest {
    void run();
}

namespace ROMLibraryTest {
    void run();
}

namespace ApplicationTest {
    void run();
}

namespace LocalGameTest {
    void run();
}

namespace LibraryPreviewTest {
    void run();
}

} // namespace PixelLink::Test::Desktop
