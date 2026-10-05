#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include <PixelLink/TestUtils/Desktop.hpp>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using namespace PixelLink::Frontend;
using namespace PixelLink::Test::Desktop::TestUtils;

namespace PixelLink::Test::Desktop::ApplicationTest {
namespace {

void testWindowAndGameLifecycle() {
    Fixture fixture("desktop-fixture");
    const auto root = fixture.path / "roms";
    const auto rom = root / UTF8Path("子目录/演示游戏.gb");
    const auto second = root / "second.gb";
    MakeROM(rom); MakeROM(second, 0x00);
    Application app({root, {}, true});
    ConfigureDrivers();
    app.Initialize();
    CHECK(app.Library().Entries().size() == 2);
    app.Draw();
    CHECK(app.SaveScreenshot(ArtifactRoot() / "library.png"));
    CHECK(app.Present());
    // Search with SDL text events, select with Enter.
    SDL_Event click{};
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN; click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 70; click.button.y = 126; click.button.clicks = 1;
    app.HandleEvent(click);
    SDL_Event text{}; text.type = SDL_EVENT_TEXT_INPUT; text.text.text = "演示游戏";
    app.HandleEvent(text);
    Key(app, SDL_SCANCODE_RETURN);
    CHECK(app.InGame());
    CHECK(app.CurrentEmulator()->Core().GetCPU().GetRegisterSnapshot().pc == 0x100);
    CHECK(app.CurrentEmulator()->IsAudioInitialized());
    CHECK(!app.CurrentEmulator()->Input().start); // Selecting a ROM isn't a game key press.
    for (int i = 0; i < 12; ++i) app.AdvanceFrame();
    CHECK(app.CurrentEmulator()->Core().GetCartridge().Read(0xA000) == 0x5A);
    app.Draw();
    CHECK(app.SaveScreenshot(ArtifactRoot() / "game.png"));
    CHECK(app.Present());
    Key(app, SDL_SCANCODE_Z);
    CHECK(app.CurrentEmulator()->Input().a);
    Key(app, SDL_SCANCODE_Z, SDL_EVENT_KEY_UP);
    CHECK(!app.CurrentEmulator()->Input().a);
    Key(app, SDL_SCANCODE_SPACE);
    CHECK(app.Paused());
    CHECK(!app.CurrentEmulator()->IsAudioInitialized());
    const auto pc = app.CurrentEmulator()->Core().GetCPU().GetRegisterSnapshot().pc;
    app.AdvanceFrame();
    CHECK(app.CurrentEmulator()->Core().GetCPU().GetRegisterSnapshot().pc == pc);
    Key(app, SDL_SCANCODE_SPACE);
    CHECK(!app.Paused());
    CHECK(app.CurrentEmulator()->IsAudioInitialized());
    Key(app, SDL_SCANCODE_M);
    CHECK(!app.CurrentEmulator()->IsAudioInitialized());
    Key(app, SDL_SCANCODE_M);
    CHECK(app.CurrentEmulator()->IsAudioInitialized());
    Key(app, SDL_SCANCODE_F5);
    auto save = rom; save.replace_extension(".sav");
    CHECK(fs::file_size(save) == 8192);
    auto* current = app.CurrentEmulator();
    CHECK(!app.OpenROM(fixture.path / "missing.gb"));
    CHECK(app.CurrentEmulator() == current); // Failed open preserves running game.
    // Save failure must neither discard the core nor close the app.
    fs::remove(save);
    fs::create_directory(save); // A directory cannot be written as a save file.
    app.CurrentEmulator()->Core().GetCartridge().Write(0xA000, 0x76);
    CHECK(!app.ExitGame());
    CHECK(app.CurrentEmulator() == current);
    SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
    app.HandleEvent(quit);
    CHECK(app.Running());
    fs::remove(save);
    CHECK(app.ExitGame());
    CHECK(!app.InGame());
    CHECK(app.OpenROM(rom));
    auto& cartridge = app.CurrentEmulator()->Core().GetCartridge();
    cartridge.Write(0x0000, 0x0A);
    CHECK(cartridge.Read(0xA000) == 0x76); // Persisted state is automatically loaded.
    CHECK(app.OpenROM(second));
    CHECK(app.CurrentEmulator()->Core().GetCPU().GetRegisterSnapshot().pc == 0x100);
    CHECK(app.CurrentEmulator()->Core().GetBus().Read(0xC000) == 0);
    SDL_Event focus{}; focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    app.HandleEvent(focus);
    CHECK(app.Paused());
    Key(app, SDL_SCANCODE_ESCAPE);
    CHECK(!app.InGame());
    CHECK(app.Library().Entries().size() == 2);
    CHECK(!fs::exists(root / rom.filename())); // Local subfolders are not flattened.
    // A sibling directory with a similar name is external, too. Run the copied
    // ROM and its existing save without writing anything back to the source.
    const auto external = fixture.path / UTF8Path("roms-other/外部游戏.gb");
    MakeROM(external);
    auto sourceSave = external; sourceSave.replace_extension(".sav");
    {
        std::ofstream output(sourceSave, std::ios::binary);
        const std::vector<char> bytes(8192, 0x37);
        output.write(bytes.data(), bytes.size());
    }
    CHECK(app.OpenROM(external));
    CHECK(fs::exists(root / external.filename()));
    CHECK(app.Library().Entries().size() == 3);
    app.CurrentEmulator()->Core().GetCartridge().Write(0x0000, 0x0A);
    CHECK(app.CurrentEmulator()->Core().GetCartridge().Read(0xA000) == 0x37);
    app.AdvanceFrame();
    CHECK(app.ExitGame());
    auto importedSave = root / external.filename(); importedSave.replace_extension(".sav");
    CHECK(std::ifstream(importedSave, std::ios::binary).get() == 0x5A);
    CHECK(std::ifstream(sourceSave, std::ios::binary).get() == 0x37);
    CHECK(app.OpenROM(external));
    CHECK(app.Library().Entries().size() == 3); // Reopening reuses the imported ROM/save.
    app.CurrentEmulator()->Core().GetCartridge().Write(0x0000, 0x0A);
    CHECK(app.CurrentEmulator()->Core().GetCartridge().Read(0xA000) == 0x5A);
    CHECK(app.ExitGame());
    CHECK(app.OpenROM(rom));
    app.AdvanceFrame();
    app.HandleEvent(quit);
    CHECK(!app.Running());
    CHECK(!app.InGame());
}

} // namespace

void run() {
    PixelLink::Test::run("SDL application / input, audio, save failures and game lifecycle", testWindowAndGameLifecycle);
}

} // namespace PixelLink::Test::Desktop::ApplicationTest
