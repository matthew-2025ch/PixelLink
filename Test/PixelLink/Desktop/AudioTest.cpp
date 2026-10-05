#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Test::Desktop::AudioTest {

void run() {
    // Share the embeddable emulator's audio check with FrontendTest.
    PixelLink::Test::Frontend::AudioTest::run();
}

} // namespace PixelLink::Test::Desktop::AudioTest
