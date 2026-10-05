#pragma once
#include <PixelLink/Frontend/Emulator.hpp>
#include <PixelLink/Frontend/ROMLibrary.hpp>
#include <PixelLink/Frontend/TextRenderer.hpp>
#include <SDL3/SDL.h>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace PixelLink::Frontend {
struct ApplicationOptions {
    std::filesystem::path romDirectory;
    std::vector<std::filesystem::path> additionalRoots;
    bool hidden = false;
};

class Application {
public:
    explicit Application(ApplicationOptions options);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    void Initialize();
    int Run();
    // These are also used by the bounded, noninteractive integration suite.
    void HandleEvent(const SDL_Event& event);
    void Draw();
    [[nodiscard]] bool Present();
    void AdvanceFrame();
    bool OpenROM(const std::filesystem::path& path);
    bool SaveGame();
    bool ExitGame();
    bool ImportROM(const std::filesystem::path& path);
    bool SaveScreenshot(const std::filesystem::path& path);
    [[nodiscard]] bool InGame() const noexcept { return emulator_ != nullptr; }
    [[nodiscard]] bool Paused() const noexcept { return paused_; }
    [[nodiscard]] bool Running() const noexcept { return running_; }
    [[nodiscard]] const ROMLibrary& Library() const noexcept { return library_; }
    [[nodiscard]] const std::string& Status() const noexcept { return status_; }
    [[nodiscard]] Emulator* CurrentEmulator() noexcept { return emulator_.get(); }
private:
    struct DialogResult {
        std::mutex mutex;
        bool done = false;
        bool failed = false;
        std::string error;
        std::vector<std::filesystem::path> files;
    };
    ApplicationOptions options_;
    ROMLibrary library_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<TextRenderer> text_;
    std::unique_ptr<Emulator> emulator_;
    // Shared ownership keeps the callback alive if the app closes mid-dialog.
    std::shared_ptr<DialogResult> dialog_;
    std::vector<std::size_t> filtered_;
    std::size_t selected_ = 0;
    std::size_t scroll_ = 0;
    std::string search_;
    std::string status_ = "选择游戏，或导入你的 ROM 文件";
    std::string gameName_;
    std::filesystem::path gamePath_;
    bool searchFocused_ = false;
    bool running_ = true;
    bool paused_ = false;
    bool muted_ = false;
    bool statusError_ = false;
    bool sdlInitialized_ = false;
    bool audioAvailable_ = false;
    bool focused_ = true;
    std::uint64_t lastAutoSave_ = 0;
    float mouseX_ = -1;
    float mouseY_ = -1;
    void Filter();
    void Refresh();
    void MoveSelection(int delta);
    void BeginImport();
    void ConsumeDialog();
    static void SDLCALL DialogCallback(void* userdata, const char* const* files, int filter);
    void LibraryClick(float x, float y, int clicks);
    void GameClick(float x, float y);
    void TogglePause();
    void ToggleMute();
    void DrawLibrary();
    void DrawGame();
    void Label(const std::string& value, float x, float y, int size = 16,
               SDL_Color color = {221, 226, 237, 255}, float width = 1000);
    void Button(const SDL_FRect& rect, const std::string& label, bool accent = false, bool enabled = true);
    void SetStatus(std::string status, bool error = false);
};
}
