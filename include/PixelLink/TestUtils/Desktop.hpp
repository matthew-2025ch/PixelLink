#pragma once

#include <PixelLink/Frontend/Application.hpp>
#include <filesystem>
#include <string>

namespace PixelLink::Test::Desktop::TestUtils {

struct Options {
    bool realAudio = false;
    bool realVideo = false;
};

void Configure(Options options);
const Options& GetOptions();
const std::filesystem::path& ArtifactRoot();
void ConfigureDrivers();

struct Fixture {
    std::filesystem::path path;
    explicit Fixture(const std::string& name);
    ~Fixture();

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
};

void MakeROM(const std::filesystem::path& path, unsigned char type = 0x03, unsigned char value = 0x5A);
void Key(PixelLink::Frontend::Application& app, SDL_Scancode key, SDL_EventType type = SDL_EVENT_KEY_DOWN);

} // namespace PixelLink::Test::Desktop::TestUtils
