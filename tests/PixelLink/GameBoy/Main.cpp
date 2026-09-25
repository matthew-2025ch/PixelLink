#include <iostream>

#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::Test::GameBoy;

int main() {
    try {
        std::cout
            << "==============================\n"
            << " Game Boy Core Tests\n"
            << "==============================\n\n";

        BusTest::run();
        CartridgeTest::run();
        CPUTest::run();
        InterruptTest::run();
        JoypadTest::run();
        MapperFactoryTest::run();
        MBC1Test::run();
        MBC3Test::run();
        MBC5Test::run();
        PPUTest::run();
        RTCTest::run();
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