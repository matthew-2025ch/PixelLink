#include <iostream>

#include <SDL3/SDL_main.h>
#include <PixelLink/Test/TestSuites.hpp>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        std::cout
            << "==============================\n"
            << " Emulator Integration Test\n"
            << "==============================\n\n";

        PixelLink::Test::Frontend::EmulatorTest::run();
    }
    catch (...) {
        std::cerr
            << "\n==============================\n"
            << " TEST FAILED\n"
            << "==============================\n";

        return 1;
    }

    std::cout
        << "\n==============================\n"
        << " TEST PASSED\n"
        << "==============================\n";

    return 0;
}
