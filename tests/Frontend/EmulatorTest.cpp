#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <thread>

#include <SDL3/SDL.h>

#include <PixelLink/Frontend/Emulator.hpp>
#include <PixelLink/Test/TestFramework.hpp>

using namespace PixelLink::Frontend;

namespace PixelLink::Test::Frontend::EmulatorTest {

namespace {

constexpr int WINDOW_SCALE = 4;

constexpr int WINDOW_WIDTH =
    Emulator::SCREEN_WIDTH * WINDOW_SCALE;

constexpr int WINDOW_HEIGHT =
    Emulator::SCREEN_HEIGHT * WINDOW_SCALE;

const std::filesystem::path ROM_PATH =
    std::filesystem::path(PIXELLINK_PROJECT_DIR) /
    LR"(assests/gb/games/For the Frogs the Bell Tolls (English).gb)";

constexpr double FRAME_SECONDS =
    70'224.0 / 4'194'304.0;

using WindowPtr =
    std::unique_ptr<
        SDL_Window,
        decltype(&SDL_DestroyWindow)
    >;

using RendererPtr =
    std::unique_ptr<
        SDL_Renderer,
        decltype(&SDL_DestroyRenderer)
    >;

class SDLGuard {
public:
    SDLGuard() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(
                std::format(
                    "SDL_Init failed: {}",
                    SDL_GetError()
                )
            );
        }
    }

    ~SDLGuard() {
        SDL_Quit();
    }

    SDLGuard(const SDLGuard&) = delete;
    SDLGuard& operator=(const SDLGuard&) = delete;
};

void testRunROM() {
    if (!std::filesystem::is_regular_file(ROM_PATH)) {
        throw std::runtime_error(
            std::format(
                "ROM file not found: {}",
                ROM_PATH.string()
            )
        );
    }

    SDLGuard sdl;

    SDL_Window* rawWindow = nullptr;
    SDL_Renderer* rawRenderer = nullptr;

    if (!SDL_CreateWindowAndRenderer(
            "PixelLink Emulator Test",
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            0,
            &rawWindow,
            &rawRenderer)) {
        throw std::runtime_error(
            std::format(
                "SDL_CreateWindowAndRenderer failed: {}",
                SDL_GetError()
            )
        );
    }

    WindowPtr window(
        rawWindow,
        SDL_DestroyWindow
    );

    RendererPtr renderer(
        rawRenderer,
        SDL_DestroyRenderer
    );

    // Emulator must be destroyed before renderer because its SDL_Texture
    // belongs to this renderer.
    Emulator emulator;

    if (!emulator.SetRenderer(renderer.get())) {
        throw std::runtime_error(
            std::format(
                "Emulator::SetRenderer failed: {}",
                SDL_GetError()
            )
        );
    }

    emulator.LoadROM(ROM_PATH);

    const SDL_FRect gameArea{
        0.0f,
        0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT)
    };

    using Clock = std::chrono::steady_clock;

    const auto frameDuration =
        std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(
                FRAME_SECONDS
            )
        );

    auto nextFrame = Clock::now();

    bool running = true;

    while (running) {
        SDL_Event event{};

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }

            emulator.HandleEvent(event);
        }

        if (!running) {
            break;
        }

        if (!emulator.RunFrame()) {
            throw std::runtime_error(
                std::format(
                    "Emulator::RunFrame failed: {}",
                    SDL_GetError()
                )
            );
        }

        if (!SDL_SetRenderDrawColor(
                renderer.get(),
                0,
                0,
                0,
                255)) {
            throw std::runtime_error(
                std::format(
                    "SDL_SetRenderDrawColor failed: {}",
                    SDL_GetError()
                )
            );
        }

        if (!SDL_RenderClear(renderer.get())) {
            throw std::runtime_error(
                std::format(
                    "SDL_RenderClear failed: {}",
                    SDL_GetError()
                )
            );
        }

        if (!emulator.Render(gameArea)) {
            throw std::runtime_error(
                std::format(
                    "Emulator::Render failed: {}",
                    SDL_GetError()
                )
            );
        }

        if (!SDL_RenderPresent(renderer.get())) {
            throw std::runtime_error(
                std::format(
                    "SDL_RenderPresent failed: {}",
                    SDL_GetError()
                )
            );
        }

        nextFrame += frameDuration;

        const auto now = Clock::now();

        if (now < nextFrame) {
            std::this_thread::sleep_until(nextFrame);
        } else if (
            now - nextFrame >
            std::chrono::milliseconds(250)
        ) {
            nextFrame = now;
        }
    }
}

} // namespace

void run() {
    Test::run(
        "Emulator / ROM / SDL",
        testRunROM
    );
}

} // namespace PixelLink::Test::Frontend::EmulatorTest
