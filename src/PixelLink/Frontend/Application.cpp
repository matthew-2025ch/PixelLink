#include <PixelLink/Frontend/Application.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

namespace PixelLink::Frontend {
namespace {
constexpr float WIDTH = 1100, HEIGHT = 720;
constexpr float LIST_TOP = 216, ROW_HEIGHT = 54;
constexpr std::size_t ROWS = 7;
constexpr SDL_Color MUTED{139, 151, 176, 255};
constexpr SDL_Color ACCENT{106, 231, 190, 255};
constexpr SDL_FRect IMPORT{808, 28, 124, 40}, REFRESH{946, 28, 126, 40};
constexpr SDL_FRect SEARCH{28, 108, 1044, 44}, PLAY{788, 538, 264, 48};
constexpr SDL_FRect MUTE{484, 20, 110, 40}, PAUSE{606, 20, 110, 40};
constexpr SDL_FRect SAVE{728, 20, 124, 40}, BACK{864, 20, 212, 40};

bool Contains(const SDL_FRect& rect, float x, float y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}
void Fill(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}
std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
bool HasSave(const PixelLink::GameBoy::CartridgeHeader& header) {
    switch (header.type) {
    case 0x03: case 0x0F: case 0x10: case 0x13: case 0x1B: case 0x1E: return true;
    default: return false;
    }
}
}

Application::Application(ApplicationOptions options)
    : options_(std::move(options)), library_(options_.romDirectory, options_.additionalRoots) {}

Application::~Application() {
    // Run() handles explicit save failures; the core destructor is only a
    // final best-effort safeguard during exceptional unwinding.
    emulator_.reset();
    text_.reset();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    if (sdlInitialized_) SDL_Quit();
}

void Application::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) throw std::runtime_error(SDL_GetError());
    sdlInitialized_ = true;
    audioAvailable_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
        (options_.hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!SDL_CreateWindowAndRenderer("PixelLink", static_cast<int>(WIDTH), static_cast<int>(HEIGHT),
                                     flags, &window_, &renderer_)) throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window_, 800, 540);
    if (!SDL_SetRenderLogicalPresentation(renderer_, static_cast<int>(WIDTH), static_cast<int>(HEIGHT),
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) throw std::runtime_error(SDL_GetError());
    text_ = std::make_unique<TextRenderer>(renderer_);
    Refresh();
    if (!audioAvailable_) SetStatus("音频设备不可用，游戏仍可运行", true);
}

void Application::SetStatus(std::string status, bool error) {
    status_ = std::move(status); statusError_ = error;
}

void Application::Filter() {
    filtered_.clear();
    const auto query = Lower(search_);
    for (std::size_t i = 0; i < library_.Entries().size(); ++i) {
        const auto& entry = library_.Entries()[i];
        if (query.empty() || Lower(entry.name + " " + entry.relativePath).find(query) != std::string::npos) filtered_.push_back(i);
    }
    selected_ = filtered_.empty() ? 0 : std::min(selected_, filtered_.size() - 1);
    scroll_ = std::min(scroll_, filtered_.size() > ROWS ? filtered_.size() - ROWS : 0);
    MoveSelection(0);
}

void Application::Refresh() {
    library_.Refresh();
    Filter();
    if (!library_.Warnings().empty()) SetStatus("部分文件无法读取：" + library_.Warnings().front(), true);
    else SetStatus(std::format("已找到 {} 个 ROM · 包含所有子文件夹", library_.Entries().size()));
}

void Application::MoveSelection(int delta) {
    if (filtered_.empty()) { selected_ = scroll_ = 0; return; }
    selected_ = static_cast<std::size_t>(std::clamp(static_cast<long long>(selected_) + delta,
        0LL, static_cast<long long>(filtered_.size() - 1)));
    if (selected_ < scroll_) scroll_ = selected_;
    if (selected_ >= scroll_ + ROWS) scroll_ = selected_ - ROWS + 1;
}

bool Application::OpenROM(const std::filesystem::path& path) {
    try {
        ROMLibrary::ValidateROM(path);
        auto localPath = std::filesystem::weakly_canonical(path);
        const auto relative = localPath.lexically_relative(std::filesystem::weakly_canonical(library_.Root()));
        // External opens (including --rom) run the library copy, so saves stay
        // beside that copy. ROMs already in subfolders keep their original path.
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
            localPath = library_.Import(path);
            Filter();
        }
        auto next = std::make_unique<Emulator>();
        next->LoadROM(localPath); // A fresh core prevents previous-game state leaking.
        if (!next->SetRenderer(renderer_)) throw std::runtime_error(SDL_GetError());
        const bool audio = audioAvailable_ && next->InitializeAudio();
        if (emulator_ && !SaveGame()) return false;
        emulator_ = std::move(next);
        gamePath_ = localPath;
        gameName_ = PathUTF8(localPath.stem());
        paused_ = false; muted_ = !audio;
        searchFocused_ = false;
        SDL_StopTextInput(window_);
        lastAutoSave_ = SDL_GetTicks();
        SDL_SetWindowTitle(window_, ("PixelLink · " + gameName_).c_str());
        SetStatus(audio ? "游戏已启动 · 存档会自动读取，退出前自动保存" : "游戏已启动，但音频设备初始化失败", !audio);
        return true;
    } catch (const std::exception& error) {
        SetStatus("无法启动游戏：" + std::string(error.what()), true);
        return false;
    }
}

bool Application::SaveGame() {
    if (!emulator_) return true;
    try {
        emulator_->Core().GetCartridge().Save();
        if (HasSave(emulator_->Core().GetCartridge().Header())) {
            SetStatus("存档已保存 · 下次进入游戏会自动读取");
        } else SetStatus("此 ROM 没有电池存档；即时状态存档暂未提供");
        lastAutoSave_ = SDL_GetTicks();
        return true;
    } catch (const std::exception& error) {
        SetStatus("保存失败，游戏仍保留：" + std::string(error.what()), true);
        return false;
    }
}

bool Application::ExitGame() {
    if (!SaveGame()) return false;
    emulator_.reset();
    paused_ = muted_ = false;
    SDL_SetWindowTitle(window_, "PixelLink");
    return true;
}

bool Application::ImportROM(const std::filesystem::path& path) {
    try {
        const auto imported = library_.Import(path);
        search_.clear(); selected_ = scroll_ = 0;
        Filter();
        for (std::size_t i = 0; i < filtered_.size(); ++i) {
            if (library_.Entries()[filtered_[i]].path == imported) { selected_ = i; break; }
        }
        MoveSelection(0);
        SetStatus("已导入：" + PathUTF8(imported.filename()));
        return true;
    } catch (const std::exception& error) {
        SetStatus("导入失败：" + PathUTF8(path.filename()) + " · " + error.what(), true);
        return false;
    }
}

void SDLCALL Application::DialogCallback(void* userdata, const char* const* files, int) {
    std::unique_ptr<std::shared_ptr<DialogResult>> owner(static_cast<std::shared_ptr<DialogResult>*>(userdata));
    auto result = *owner;
    std::lock_guard guard(result->mutex);
    try {
        if (!files) {
            result->failed = true; result->error = SDL_GetError();
        } else {
            for (std::size_t i = 0; files[i]; ++i) result->files.push_back(UTF8Path(files[i]));
        }
    } catch (const std::exception& error) {
        result->failed = true; result->error = error.what();
    }
    result->done = true;
}

void Application::BeginImport() {
    if (dialog_) return;
    static const SDL_DialogFileFilter filters[]{{"Game Boy ROM", "gb;gbc;rom;bin"}};
    dialog_ = std::make_shared<DialogResult>();
    SDL_ShowOpenFileDialog(DialogCallback, new std::shared_ptr<DialogResult>(dialog_), window_,
                          filters, 1, nullptr, true);
    SetStatus("请选择要导入的 ROM · 支持一次选择多个文件");
}

void Application::ConsumeDialog() {
    if (!dialog_) return;
    auto result = dialog_;
    std::vector<std::filesystem::path> files;
    std::string error;
    bool failed = false;
    {
        std::lock_guard guard(result->mutex);
        if (!result->done) return;
        files = std::move(result->files);
        failed = result->failed; error = result->error;
    }
    dialog_.reset();
    if (failed) { SetStatus("文件选择失败：" + error, true); return; }
    if (files.empty()) { SetStatus("已取消导入"); return; }
    std::size_t successes = 0;
    std::string firstError;
    for (const auto& file : files) {
        if (ImportROM(file)) ++successes;
        else if (firstError.empty()) firstError = status_;
    }
    SetStatus(std::format("导入完成：{} 成功 / {} 失败", successes, files.size() - successes) +
        (firstError.empty() ? "" : " · " + firstError), successes != files.size());
}

void Application::TogglePause() {
    if (!emulator_) return;
    paused_ = !paused_;
    emulator_->ClearInput();
    // Dropping the old audio stream prevents stale sound on pause/resume.
    emulator_->ShutdownAudio();
    if (!paused_ && !muted_ && !emulator_->InitializeAudio()) {
        muted_ = true; SetStatus("声音恢复失败：" + std::string(SDL_GetError()), true);
    } else SetStatus(paused_ ? "游戏已暂停" : "游戏已继续");
}

void Application::ToggleMute() {
    if (!emulator_) return;
    if (!muted_) { muted_ = true; emulator_->ShutdownAudio(); SetStatus("已静音"); }
    else if (paused_ || emulator_->InitializeAudio()) { muted_ = false; SetStatus("声音已开启"); }
    else SetStatus("无法打开音频设备：" + std::string(SDL_GetError()), true);
}

void Application::LibraryClick(float x, float y, int clicks) {
    if (Contains(SEARCH, x, y)) {
        searchFocused_ = true; SDL_StartTextInput(window_); return;
    }
    searchFocused_ = false; SDL_StopTextInput(window_);
    if (Contains(IMPORT, x, y)) BeginImport();
    else if (Contains(REFRESH, x, y)) Refresh();
    else if (Contains(PLAY, x, y) && !filtered_.empty()) OpenROM(library_.Entries()[filtered_[selected_]].path);
    else if (x >= 28 && x < 740 && y >= LIST_TOP && y < LIST_TOP + ROWS * ROW_HEIGHT) {
        const auto index = scroll_ + static_cast<std::size_t>((y - LIST_TOP) / ROW_HEIGHT);
        if (index < filtered_.size()) {
            selected_ = index;
            if (clicks >= 2) OpenROM(library_.Entries()[filtered_[selected_]].path);
        }
    }
}

void Application::GameClick(float x, float y) {
    if (Contains(MUTE, x, y)) ToggleMute();
    else if (Contains(PAUSE, x, y)) TogglePause();
    else if (Contains(SAVE, x, y)) SaveGame();
    else if (Contains(BACK, x, y)) ExitGame();
}

void Application::HandleEvent(const SDL_Event& original) {
    const bool wasInGame = InGame();
    SDL_Event event = original;
    SDL_ConvertEventToRenderCoordinates(renderer_, &event);
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (!dialog_ && ExitGame()) running_ = false;
        else if (dialog_) SetStatus("请先关闭文件选择窗口，再退出应用");
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        focused_ = false;
        if (emulator_) {
            emulator_->ClearInput();
            if (!paused_) TogglePause();
        }
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) focused_ = true;
    if (event.type == SDL_EVENT_MOUSE_MOTION) { mouseX_ = event.motion.x; mouseY_ = event.motion.y; }
    if (event.type == SDL_EVENT_DROP_FILE && event.drop.data && !emulator_) ImportROM(UTF8Path(event.drop.data));
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (emulator_) GameClick(event.button.x, event.button.y);
        else LibraryClick(event.button.x, event.button.y, event.button.clicks);
    }
    if (!emulator_ && event.type == SDL_EVENT_MOUSE_WHEEL) {
        MoveSelection(-static_cast<int>(std::round(event.wheel.y)) * 3);
    }
    if (!emulator_ && searchFocused_ && event.type == SDL_EVENT_TEXT_INPUT && event.text.text) {
        if (search_.size() < 256) search_ += event.text.text;
        selected_ = scroll_ = 0; Filter();
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        const auto key = event.key.scancode;
        if (emulator_) {
            if (key == SDL_SCANCODE_ESCAPE) { ExitGame(); return; }
            if (key == SDL_SCANCODE_F5) { SaveGame(); return; }
            if (key == SDL_SCANCODE_SPACE) { TogglePause(); return; }
            if (key == SDL_SCANCODE_M) { ToggleMute(); return; }
        } else {
            if ((event.key.mod & SDL_KMOD_CTRL) && key == SDL_SCANCODE_O) BeginImport();
            else if (key == SDL_SCANCODE_F5) Refresh();
            else if (key == SDL_SCANCODE_UP) MoveSelection(-1);
            else if (key == SDL_SCANCODE_DOWN) MoveSelection(1);
            else if (key == SDL_SCANCODE_PAGEUP) MoveSelection(-static_cast<int>(ROWS));
            else if (key == SDL_SCANCODE_PAGEDOWN) MoveSelection(static_cast<int>(ROWS));
            else if (key == SDL_SCANCODE_RETURN && !filtered_.empty()) OpenROM(library_.Entries()[filtered_[selected_]].path);
            else if (key == SDL_SCANCODE_ESCAPE) {
                search_.clear(); searchFocused_ = false; SDL_StopTextInput(window_); Filter();
            } else if (key == SDL_SCANCODE_BACKSPACE && searchFocused_ && !search_.empty()) {
                auto offset = search_.size() - 1;
                while (offset > 0 && (static_cast<unsigned char>(search_[offset]) & 0xC0) == 0x80) --offset;
                search_.erase(offset); Filter();
            }
        }
    }
    if (wasInGame && emulator_ && !paused_ && focused_) emulator_->HandleEvent(event);
}

void Application::AdvanceFrame() {
    ConsumeDialog();
    if (emulator_ && !paused_) {
        try {
            if (!emulator_->RunFrame()) throw std::runtime_error(SDL_GetError());
        } catch (const std::exception& error) {
            TogglePause();
            SetStatus("游戏已暂停：" + std::string(error.what()), true);
        }
    }
    if (emulator_ && SDL_GetTicks() - lastAutoSave_ >= 30'000) {
        SaveGame();
        lastAutoSave_ = SDL_GetTicks(); // Failed saves wait before retrying.
    }
}

void Application::Label(const std::string& value, float x, float y, int size, SDL_Color color, float width) {
    text_->Draw(value, x, y, size, color, width);
}

void Application::Button(const SDL_FRect& rect, const std::string& label, bool accent, bool enabled) {
    const bool hover = enabled && Contains(rect, mouseX_, mouseY_);
    Fill(renderer_, rect, enabled && accent ? (hover ? SDL_Color{136, 246, 208, 255} : ACCENT) :
        (hover ? SDL_Color{52, 64, 85, 255} : SDL_Color{35, 44, 61, 255}));
    Label(label, rect.x + 16, rect.y + (rect.h - 24) / 2, 16,
        enabled ? (accent ? SDL_Color{14, 37, 32, 255} : SDL_Color{221, 226, 237, 255}) : MUTED, rect.w - 24);
}

void Application::DrawLibrary() {
    Label("PixelLink", 28, 24, 32);
    Label("你的 Game Boy 游戏库", 30, 69, 16, MUTED);
    Button(IMPORT, dialog_ ? "选择中…" : "导入 ROM", true, !dialog_);
    Button(REFRESH, "刷新列表");
    Fill(renderer_, SEARCH, searchFocused_ ? SDL_Color{38, 53, 66, 255} : SDL_Color{27, 35, 50, 255});
    Label(search_.empty() ? "搜索游戏名称或子文件夹…" : search_ + (searchFocused_ ? "|" : ""),
        46, 118, 17, search_.empty() ? MUTED : SDL_Color{221, 226, 237, 255}, 1000);
    Fill(renderer_, {28, 176, 712, 448}, {23, 30, 44, 255});
    Label(std::format("全部游戏  {} / {}", filtered_.size(), library_.Entries().size()), 46, 187, 15, MUTED);
    for (std::size_t row = 0; row < ROWS; ++row) {
        const auto index = scroll_ + row;
        if (index >= filtered_.size()) break;
        const auto& entry = library_.Entries()[filtered_[index]];
        const float y = LIST_TOP + row * ROW_HEIGHT;
        const SDL_FRect rect{36, y, 690, ROW_HEIGHT - 4};
        if (index == selected_) {
            Fill(renderer_, rect, {40, 65, 66, 255});
            Fill(renderer_, {36, y + 8, 3, 32}, ACCENT);
        } else if (Contains(rect, mouseX_, mouseY_)) Fill(renderer_, rect, {33, 43, 60, 255});
        Label(entry.name, 50, y + 3, 17, {227, 232, 242, 255}, 566);
        Label(entry.relativePath, 50, y + 27, 12, MUTED, 566);
        Label(std::format("{} KB", (entry.size + 1023) / 1024), 630, y + 15, 13, MUTED, 80);
    }
    if (filtered_.empty()) {
        Label(search_.empty() ? "还没有 ROM" : "没有找到匹配的游戏", 72, 304, 24);
        Label(search_.empty() ? "点击右上角导入，或把 ROM 拖入窗口。" : "换个关键词，或按 Esc 清空搜索。", 72, 350, 16, MUTED, 620);
    }
    if (filtered_.size() > ROWS) {
        const float thumb = 360 * ROWS / filtered_.size();
        const float y = LIST_TOP + (360 - thumb) * scroll_ / (filtered_.size() - ROWS);
        Fill(renderer_, {731, y, 3, thumb}, {84, 105, 120, 255});
    }
    Fill(renderer_, {768, 176, 304, 448}, {23, 30, 44, 255});
    Label("准备开始", 788, 199, 16, ACCENT);
    // A simple Game Boy silhouette, built entirely with SDL shapes.
    Fill(renderer_, {866, 247, 106, 146}, {59, 76, 86, 255});
    Fill(renderer_, {878, 261, 82, 68}, {14, 27, 28, 255});
    Fill(renderer_, {888, 271, 62, 48}, {165, 198, 149, 255});
    Fill(renderer_, {880, 349, 30, 10}, {17, 27, 38, 255});
    Fill(renderer_, {890, 339, 10, 30}, {17, 27, 38, 255});
    Fill(renderer_, {934, 344, 12, 12}, ACCENT);
    Fill(renderer_, {951, 334, 12, 12}, ACCENT);
    if (!filtered_.empty()) {
        const auto& entry = library_.Entries()[filtered_[selected_]];
        Label(entry.name, 788, 416, 20, {221, 226, 237, 255}, 264);
        Label(entry.relativePath, 788, 452, 13, MUTED, 264);
        Label(std::format("{} KB · 自动读取电池存档", (entry.size + 1023) / 1024), 788, 490, 14, MUTED, 264);
    } else Label("导入你的第一款游戏", 788, 429, 19, MUTED, 264);
    Button(PLAY, "开始游戏    Enter", true, !filtered_.empty());
    Label("双击进入 · ↑↓ 选择 · 滚轮翻页 · Ctrl+O 导入", 28, 642, 15, MUTED, 1044);
}

void Application::DrawGame() {
    Label(gameName_, 24, 22, 24, {221, 226, 237, 255}, 440);
    Label(paused_ ? "已暂停" : "Game Boy · 立体声", 26, 56, 13, paused_ ? ACCENT : MUTED, 430);
    Button(MUTE, muted_ ? "开启声音" : "静音  M");
    Button(PAUSE, paused_ ? "继续游戏" : "暂停游戏");
    Button(SAVE, "保存  F5", true);
    Button(BACK, "保存并返回游戏库");
    Fill(renderer_, {24, 92, 1052, 522}, {8, 13, 20, 255});
    constexpr SDL_FRect screen{310, 137, 480, 432};
    if (!emulator_->Render(screen)) SetStatus("画面显示失败：" + std::string(SDL_GetError()), true);
    if (paused_) {
        Fill(renderer_, {382, 306, 336, 86}, {25, 40, 51, 244});
        Label("游戏已暂停", 478, 316, 22);
        Label("按空格或点击继续游戏", 450, 350, 15, MUTED);
    }
    Label("方向键 移动    Z / X 操作    Enter 开始    Backspace 选择", 28, 635, 16, MUTED, 1044);
    Label("空格 暂停    M 静音    F5 保存    Esc 保存并返回", 28, 660, 14, MUTED, 1044);
}

void Application::Draw() {
    SDL_SetRenderDrawColor(renderer_, 15, 21, 33, 255);
    SDL_RenderClear(renderer_);
    text_->BeginFrame();
    if (emulator_) DrawGame(); else DrawLibrary();
    Label(status_, 28, 692, 13, statusError_ ? SDL_Color{255, 157, 153, 255} : ACCENT, 1044);
    text_->EndFrame();
}

bool Application::SaveScreenshot(const std::filesystem::path& path) {
    auto* surface = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surface) return false;
    const bool ok = path.extension() == ".png" ? SDL_SavePNG(surface, PathUTF8(path).c_str()) :
                                                SDL_SaveBMP(surface, PathUTF8(path).c_str());
    SDL_DestroySurface(surface);
    return ok;
}

bool Application::Present() {
    return SDL_RenderPresent(renderer_);
}

int Application::Run() {
    const auto frameNanoseconds = static_cast<Uint64>(1'000'000'000.0 * 70'224 / 4'194'304);
    auto nextFrame = SDL_GetTicksNS();
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) HandleEvent(event);
        if (!running_) break;
        AdvanceFrame(); Draw();
        if (!Present()) throw std::runtime_error(SDL_GetError());
        nextFrame += frameNanoseconds;
        const auto now = SDL_GetTicksNS();
        if (now < nextFrame) SDL_DelayNS(nextFrame - now);
        else if (now - nextFrame > frameNanoseconds * 4) nextFrame = now;
    }
    return 0;
}
}
