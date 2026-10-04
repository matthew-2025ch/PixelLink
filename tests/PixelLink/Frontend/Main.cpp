#include <iostream>

#include <SDL3/SDL_main.h>
#include <PixelLink/Test/TestSuites.hpp>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        std::cout
            << "==============================\n"
            << " Frontend Integration Tests\n"
            << "==============================\n\n";

        PixelLink::Test::Frontend::AudioTest::run();
        PixelLink::Test::Frontend::EmulatorTest::run();
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
