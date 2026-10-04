#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

#include <PixelLink/Frontend/Emulator.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Test::Frontend::AudioTest {
namespace {

class ScopedDummyAudio {
public:
    ScopedDummyAudio() {
        if (const auto* driver = SDL_GetHint(SDL_HINT_AUDIO_DRIVER)) {
            previousDriver_ = driver;
        }
        if (!SDL_SetHintWithPriority(
                SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE)) {
            throw std::runtime_error(std::format(
                "SDL audio driver selection failed: {}", SDL_GetError()));
        }
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            const auto error = std::format(
                "SDL audio init failed: {}", SDL_GetError());
            RestoreDriver();
            throw std::runtime_error(error);
        }
    }

    ~ScopedDummyAudio() {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        RestoreDriver();
    }

    ScopedDummyAudio(const ScopedDummyAudio&) = delete;
    ScopedDummyAudio& operator=(const ScopedDummyAudio&) = delete;

private:
    std::optional<std::string> previousDriver_;

    void RestoreDriver() noexcept {
        // Restore the selected driver on success and on exception so the
        // following interactive game uses the user's real audio backend.
        if (previousDriver_) {
            SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,
                previousDriver_->c_str(), SDL_HINT_OVERRIDE);
        } else {
            SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
        }
    }
};

void testAutomaticAudioPlayback() {
    using PixelLink::Frontend::Emulator;

    ScopedDummyAudio audio;
    Emulator emulator;
    CHECK(emulator.InitializeAudio());
    CHECK(emulator.IsAudioInitialized());
    CHECK(!emulator.PushAudio(std::array<float, 1>{0.0f}));

    // No renderer or window is needed for APU output to reach SDL.
    auto& bus = emulator.Core().GetBus();
    bus.Write(0xFF25, 0x22);
    bus.Write(0xFF16, 0x80);
    bus.Write(0xFF17, 0xF0);
    bus.Write(0xFF18, 0x00);
    bus.Write(0xFF19, 0x87);
    CHECK(emulator.RunFrame());
    CHECK(emulator.Core().GetAPU().TakeSamples().empty());

    emulator.ShutdownAudio();
    CHECK(!emulator.IsAudioInitialized());
    CHECK(!emulator.PushAudio(std::array<float, 2>{0.0f, 0.0f}));

    CHECK(emulator.InitializeAudio({24'000, 1}));
    CHECK(emulator.RunFrame());
    emulator.ShutdownAudio();
    CHECK(!emulator.InitializeAudio({0, 2}));
}

} // namespace

void run() {
    Test::run("Frontend / automatic APU audio", testAutomaticAudioPlayback);
}

} // namespace PixelLink::Test::Frontend::AudioTest
