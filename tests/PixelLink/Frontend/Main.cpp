#include <SDL3/SDL_main.h>
#include <PixelLink/Test/Logger.hpp>
#include <PixelLink/Test/TestSuites.hpp>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    return PixelLink::Test::RunTestProgram("FrontendTests", "Frontend Integration Tests", [] {
        PixelLink::Test::Frontend::AudioTest::run();
        PixelLink::Test::Frontend::EmulatorTest::run();
    });
}
