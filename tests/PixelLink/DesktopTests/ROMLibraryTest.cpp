#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include "TestUtils.hpp"
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace PixelLink::Frontend;
using namespace PixelLink::Tests::Desktop::TestUtils;

namespace PixelLink::Tests::Desktop::ROMLibraryTest {
namespace {

void testRecursiveLibraryAndImport() {
    Fixture fixture("library-fixture");
    const auto root = fixture.path / "roms";
    const auto original = root / UTF8Path("子文件夹/演示.GB");
    MakeROM(original);
    MakeROM(root / "second.gbc", 0x00);
    std::ofstream(root / "ignored.sav") << "save data";
    ROMLibrary library(root, {root});
    library.Refresh();
    CHECK(library.Entries().size() == 2); // Dedup roots, recurse, ignore saves.
    CHECK(library.Warnings().empty());
    CHECK(PathUTF8(UTF8Path("子文件夹/演示.GB")) == "子文件夹/演示.GB");
    const auto imported = library.Import(original);
    CHECK(imported.parent_path() == fs::weakly_canonical(root));
    CHECK(library.Entries().size() == 3);
    CHECK(library.Import(original) == imported); // Identical import deduplicates.
    MakeROM(fixture.path / UTF8Path("outside/演示.GB"), 0x03, 0x73);
    const auto other = library.Import(fixture.path / UTF8Path("outside/演示.GB"));
    CHECK(other != imported);
    CHECK(PathUTF8(other.stem()).find("(1)") != std::string::npos);
    const auto source = fixture.path / "outside/another.gb";
    MakeROM(source);
    std::ofstream(root / "another.sav") << "orphaned save";
    CHECK(library.Import(source).filename() == "another (1).gb");
    const auto invalid = fixture.path / "bad.gb";
    std::ofstream(invalid) << "not a ROM";
    bool rejected = false;
    try { (void)library.Import(invalid); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
    CHECK(!fs::exists(root / "bad.gb"));
}

} // namespace

void run() {
    PixelLink::Test::run("ROM library / recursive scan, Unicode and safe imports", testRecursiveLibraryAndImport);
}

} // namespace PixelLink::Tests::Desktop::ROMLibraryTest
