#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include <PixelLink/TestUtils/Desktop.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace fs = std::filesystem;
using namespace PixelLink::Frontend;
using namespace PixelLink::Test::Desktop::TestUtils;

namespace PixelLink::Test::Desktop::LocalGameTest {
namespace {

void testLocalGameAudioAndRendering() {
    Fixture fixture("local-game-fixture");
    const auto project = fs::path(PIXELLINK_PROJECT_DIR);
    const std::array<const char*, 2> names{
        "For the Frogs the Bell Tolls (English).gb",
        "Super Breakout! (Europe) (En,Fr,De,Es,It,Nl) (GB Compatible).gbc"
    };
    Application app({fixture.path, {}, true});
    ConfigureDrivers();
    app.Initialize();
    for (const auto* name : names) {
        const auto source = project / "assests/roms" / name;
        const auto copy = fixture.path / name;
        fs::copy_file(source, copy);
        CHECK(app.OpenROM(copy));
        CHECK(app.CurrentEmulator()->IsAudioInitialized());
        // Real-device mode runs a paced ten-second playback for each game.
        // Dummy mode still checks the same sample-generation path quickly.
        bool signal = false;
        bool left = false, right = false;
        double peak = 0;
        auto& core = app.CurrentEmulator()->Core();
        core.GetAPU().SetSampleCaptureEnabled(true);
        auto nextFrame = SDL_GetTicksNS();
        const auto frameDuration = static_cast<Uint64>(1'000'000'000.0 * 70'224 / 4'194'304);
        for (int frame = 0; frame < 600; ++frame) {
            // Inspect generated PCM before forwarding it through SDL.
            std::uint32_t cycles = 0;
            while (cycles < 70'224) cycles += core.Step();
            const auto samples = core.GetAPU().TakeSamples();
            CHECK(!samples.empty());
            CHECK(std::all_of(samples.begin(), samples.end(), [](float value) {
                return std::isfinite(value) && std::abs(value) <= 1.0f;
            }));
            for (std::size_t i = 0; i < samples.size(); i += 2) {
                left |= std::abs(samples[i]) > 0.001f;
                right |= std::abs(samples[i + 1]) > 0.001f;
                peak = std::max(peak, static_cast<double>(std::max(std::abs(samples[i]), std::abs(samples[i + 1]))));
            }
            signal |= left || right;
            CHECK(app.CurrentEmulator()->PushAudio(samples));
            if (frame == 180 || frame == 300 || frame == 420) Key(app, SDL_SCANCODE_RETURN);
            if (frame == 182 || frame == 302 || frame == 422) Key(app, SDL_SCANCODE_RETURN, SDL_EVENT_KEY_UP);
            if (frame == 240 || frame == 360) Key(app, SDL_SCANCODE_Z);
            if (frame == 242 || frame == 362) Key(app, SDL_SCANCODE_Z, SDL_EVENT_KEY_UP);
            if (GetOptions().realAudio) {
                nextFrame += frameDuration;
                const auto now = SDL_GetTicksNS();
                if (now < nextFrame) SDL_DelayNS(nextFrame - now);
                else if (now - nextFrame > frameDuration * 4) nextFrame = now;
            }
        }
        PixelLink::Utils::GetLogger().info("Local game audio: {}, driver={}, left={}, right={}, peak={}",
            name, SDL_GetCurrentAudioDriver(), left, right, peak);
        app.AdvanceFrame(); // Synchronize the real game framebuffer to SDL.
        app.Draw();
        CHECK(app.SaveScreenshot(ArtifactRoot() / (std::string(name).find("Frogs") != std::string::npos ? "frogs.png" : "breakout.png")));
        CHECK(app.Present());
        if (std::string(name).find("Frogs") != std::string::npos) {
            CHECK(signal);
            CHECK(left && right);
        }
        CHECK(app.ExitGame());
    }
}

} // namespace

void run() {
    PixelLink::Test::run("SDL application / local game PCM and rendering", testLocalGameAudioAndRendering);
}

} // namespace PixelLink::Test::Desktop::LocalGameTest
