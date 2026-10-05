#include <PixelLink/TestUtils/Desktop.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace PixelLink::Frontend;

namespace PixelLink::Test::Desktop::TestUtils {
namespace {
Options options;
}

void Configure(Options selected) {
    options = selected;
    options.realAudio |= options.realVideo;
    fs::create_directories(ArtifactRoot());
}

const Options& GetOptions() { return options; }

const fs::path& ArtifactRoot() {
    static const fs::path root = fs::path(PIXELLINK_DESKTOP_TEST_DIR);
    return root;
}

void ConfigureDrivers() {
    // SDL_Quit clears hints, so configure each independent application.
    if (!options.realVideo) {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    }
    if (!options.realAudio) SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
}

Fixture::Fixture(const std::string& name) {
    path = ArtifactRoot() / UTF8Path(name + "-" + std::to_string(SDL_GetTicksNS()));
    fs::create_directories(path);
}

Fixture::~Fixture() {
    // Fixtures are fresh children of the configured build directory.
    // Retain screenshots/logs; remove only generated ROM/save data.
    std::error_code ec;
    fs::remove_all(path, ec);
}

void MakeROM(const fs::path& path, unsigned char type, unsigned char value) {
    fs::create_directories(path.parent_path());
    std::vector<unsigned char> rom(32768);
    rom[0x100] = 0xC3; rom[0x101] = 0x50; rom[0x102] = 0x01;
    rom[0x147] = type;
    rom[0x149] = type == 0x03 ? 2 : 0;
    // Enable RAM, store a byte, start CH2, then loop forever.
    const std::vector<unsigned char> program{
        0x3E, 0x0A, 0xEA, 0x00, 0x00,
        0x3E, value, 0xEA, 0x00, 0xA0,
        0x3E, 0x80, 0xE0, 0x16,
        0x3E, 0xF0, 0xE0, 0x17,
        0x3E, 0x00, 0xE0, 0x18,
        0x3E, 0x87, 0xE0, 0x19,
        0x18, 0xFE
    };
    std::copy(program.begin(), program.end(), rom.begin() + 0x150);
    unsigned char checksum = 0;
    for (int i = 0x134; i <= 0x14C; ++i) checksum = checksum - rom[i] - 1;
    rom[0x14D] = checksum;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(rom.data()), rom.size());
    if (!file) throw std::runtime_error("Cannot create test ROM");
}

void Key(Application& app, SDL_Scancode key, SDL_EventType type) {
    SDL_Event event{}; event.type = type; event.key.scancode = key; event.key.repeat = false;
    app.HandleEvent(event);
}

} // namespace PixelLink::Test::Desktop::TestUtils
