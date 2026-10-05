#include <SDL3/SDL_main.h>
#include <PixelLink/Test/TestRunner.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include <string>

#include <PixelLink/TestUtils/Desktop.hpp>

using namespace PixelLink::Test::Desktop;

int main(int argc, char* argv[]) {
    const bool realVideo = argc > 1 && std::string(argv[1]) == "--real-devices";
    const bool realAudio = realVideo || (argc > 1 && std::string(argv[1]) == "--real-audio");

    return PixelLink::Test::RunTestProgram("DesktopTest", "SDL Desktop Tests", [=] {
        TestUtils::Configure({realAudio, realVideo});
        AudioTest::run();
        ROMLibraryTest::run();
        ApplicationTest::run();
        LocalGameTest::run();
        LibraryPreviewTest::run();
    });
}
