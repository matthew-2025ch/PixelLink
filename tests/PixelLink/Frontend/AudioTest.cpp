#include <array>
#include <iostream>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <PixelLink/Frontend/Emulator.hpp>
#include <PixelLink/Test/TestFramework.hpp>

namespace {

void testAutomaticAudioPlayback() {
    using PixelLink::Frontend::Emulator;

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

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    int result = 0;
    try {
        PixelLink::Test::run("Frontend / automatic APU audio", testAutomaticAudioPlayback);
    } catch (const std::exception&) {
        result = 1;
    }
    SDL_Quit();
    return result;
}
