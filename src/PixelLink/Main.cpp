#include <PixelLink/Frontend/Application.hpp>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    using namespace PixelLink::Frontend;
    try {
        const auto project = std::filesystem::path(PIXELLINK_PROJECT_DIR);
        ApplicationOptions options{project / "assests/roms", {}};
        std::filesystem::path initialROM;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--rom-dir" && i + 1 < argc) {
                options.romDirectory = UTF8Path(argv[++i]); options.additionalRoots.clear();
            } else if (argument == "--rom" && i + 1 < argc) initialROM = UTF8Path(argv[++i]);
            else throw std::runtime_error("Usage: PixelLink [--rom-dir DIRECTORY] [--rom FILE]");
        }
        Application app(std::move(options));
        app.Initialize();
        if (!initialROM.empty()) app.OpenROM(initialROM);
        return app.Run();
    } catch (const std::exception& error) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PixelLink", error.what(), nullptr);
        return 1;
    }
}
