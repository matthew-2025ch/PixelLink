#include <iostream>

#include "TestSuites.hpp"

int main() {
    try {
        std::cout
            << "==============================\n"
            << " Game Boy Emulator Tests\n"
            << "==============================\n\n";

        PixelLink::Test::GameBoy::CPUTest::run();
        PixelLink::Test::GameBoy::InterruptTest::run();
        PixelLink::Test::GameBoy::CartridgeTest::run();
        PixelLink::Test::GameBoy::BusTest::run();
        PixelLink::Test::GameBoy::TimerTest::run();
        PixelLink::Test::GameBoy::TimerIntegrationTest::run();
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