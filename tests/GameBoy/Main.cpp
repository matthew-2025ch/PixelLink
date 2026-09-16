#include <iostream>

#include <PixelLink/Test/TestSuites.hpp>

int main() {
    try {
        std::cout
            << "==============================\n"
            << " Game Boy Core Tests\n"
            << "==============================\n\n";

        PixelLink::Test::GameBoy::BusTest::run();
        PixelLink::Test::GameBoy::CartridgeTest::run();
        PixelLink::Test::GameBoy::CPUTest::run();
        PixelLink::Test::GameBoy::InterruptTest::run();
        PixelLink::Test::GameBoy::JoypadTest::run();
        PixelLink::Test::GameBoy::PPUTest::run();
        PixelLink::Test::GameBoy::RTCTest::run();
        PixelLink::Test::GameBoy::TimerIntegrationTest::run();
        PixelLink::Test::GameBoy::TimerTest::run();
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