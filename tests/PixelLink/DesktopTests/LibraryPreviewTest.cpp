#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include "TestUtils.hpp"

namespace fs = std::filesystem;
using namespace PixelLink::Frontend;
using namespace PixelLink::Tests::Desktop::TestUtils;

namespace PixelLink::Tests::Desktop::LibraryPreviewTest {
namespace {

void testProjectLibraryPreview() {
    const auto project = fs::path(PIXELLINK_PROJECT_DIR);
    Application app({project / "assests/roms", {}, true});
    ConfigureDrivers();
    app.Initialize();
    CHECK(app.Library().Entries().size() >= 52); // 40 Mooneye + 12 APU tests.
    app.Draw();
    CHECK(app.SaveScreenshot(ArtifactRoot() / "project-library.png"));
    CHECK(app.Present());
    PixelLink::Test::GetLogger().info("SDL video driver: {}", SDL_GetCurrentVideoDriver());
}

} // namespace

void run() {
    PixelLink::Test::run("SDL application / actual project library preview", testProjectLibraryPreview);
}

} // namespace PixelLink::Tests::Desktop::LibraryPreviewTest
