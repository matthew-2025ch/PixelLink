#include <iostream>

#include "TestSuites.hpp"

int main() {
    try {
        std::cout
            << "==============================\n"
            << " Game Boy Emulator Tests\n"
            << "==============================\n\n";

        CPUTest::run();
        InterruptTest::run();
        CartridgeTest::run();
        BusTest::run();
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
