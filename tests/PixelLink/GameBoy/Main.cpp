#include <iostream>

#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::Test::GameBoy;

int main() {
    try {
        std::cout
            << "==============================\n"
            << " Game Boy Core Tests\n"
            << "==============================\n\n";

        APUTest::run();
        BusTest::run();
        CartridgeTest::run();
        CPUTest::run();
        CoreTimingTest::run();
        InterruptTest::run();
        JoypadTest::run();
        MapperFactoryTest::run();
        MBC1Test::run();
        MBC3Test::run();
        MBC5Test::run();
        PPUTest::run();
        ROMTest::run();
        RTCTest::run();
        SaveTest::run();
        TimerIntegrationTest::run();
        TimerTest::run();
    }
    catch (...) {
        std::cerr
            << "\n==============================\n"
            << " TESTS FAILED\n"
            << "==============================\n";

        return 1;
    }

    std::cout
        << "\n==============================\n"
        << " ALL TESTS PASSED\n"
        << "==============================\n";

    return 0;
}
