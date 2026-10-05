#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Tests::Desktop::AudioTest {

void run() {
    // Share the embeddable emulator's audio check with FrontendTests.
    PixelLink::Test::Frontend::AudioTest::run();
}

} // namespace PixelLink::Tests::Desktop::AudioTest
