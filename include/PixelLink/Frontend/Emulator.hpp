#pragma once

#include <PixelLink/GameBoy/GameBoy.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>

struct SDL_AudioStream;
union SDL_Event;
struct SDL_FRect;
struct SDL_Renderer;
struct SDL_Texture;

namespace PixelLink::Frontend {

/**
 * Embeddable local Game Boy emulator component.
 *
 * Emulator owns:
 *   - the Game Boy core
 *   - the SDL texture used to display the PPU framebuffer
 *   - the SDL audio stream used for playback
 *   - local keyboard input state
 *
 * Emulator does NOT own:
 *   - SDL initialization/shutdown
 *   - SDL_Window
 *   - SDL_Renderer lifetime
 *   - SDL_PollEvent()
 *   - SDL_RenderClear()
 *   - SDL_RenderPresent()
 *
 * The host application supplies an SDL_Renderer and decides where the
 * emulator is rendered inside its window.
 */
class Emulator {
public:
    struct InputState {
        bool a = false;
        bool b = false;
        bool start = false;
        bool select = false;

        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    };

    struct AudioConfig {
        int sampleRate = 48'000;
        int channels = 2;
    };

    static constexpr int SCREEN_WIDTH =
        static_cast<int>(PixelLink::GameBoy::PPU::SCREEN_WIDTH);

    static constexpr int SCREEN_HEIGHT =
        static_cast<int>(PixelLink::GameBoy::PPU::SCREEN_HEIGHT);

    Emulator() = default;
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    Emulator(Emulator&&) = delete;
    Emulator& operator=(Emulator&&) = delete;

    // ---------------------------------------------------------------------
    // Core
    // ---------------------------------------------------------------------

    void LoadROM(const std::filesystem::path& path);

    // Execute one nominal DMG frame (70,224 t-cycles) and synchronize the
    // latest PPU framebuffer with the SDL texture when a renderer is set.
    //
    // Returns false only when SDL video synchronization fails.
    // The Game Boy core is still advanced even when no renderer is attached.
    [[nodiscard]] bool RunFrame();

    // Exposed primarily for debugging and tests. Normal application code
    // should prefer Emulator's higher-level interface.
    [[nodiscard]] PixelLink::GameBoy::GameBoy& Core() noexcept;
    [[nodiscard]] const PixelLink::GameBoy::GameBoy& Core() const noexcept;

    // ---------------------------------------------------------------------
    // Video
    // ---------------------------------------------------------------------

    // The renderer is non-owning and must outlive Emulator, or be detached
    // with ClearRenderer() before the renderer is destroyed.
    [[nodiscard]] bool SetRenderer(SDL_Renderer* renderer) noexcept;
    void ClearRenderer() noexcept;

    // Render the most recently completed Game Boy frame into an arbitrary
    // region of the host renderer.
    //
    // This function does NOT call SDL_RenderClear() or SDL_RenderPresent().
    [[nodiscard]] bool Render(const SDL_FRect& destination) const noexcept;

    // ---------------------------------------------------------------------
    // Audio
    // ---------------------------------------------------------------------

    // The host must initialize SDL audio before calling this.
    [[nodiscard]] bool InitializeAudio(
        const AudioConfig& config = {}
    ) noexcept;

    void ShutdownAudio() noexcept;

    // Temporary bridge until the Game Boy APU exists.
    // Samples are interleaved 32-bit floating-point PCM.
    [[nodiscard]] bool PushAudio(
        std::span<const float> interleavedSamples
    ) noexcept;

    // ---------------------------------------------------------------------
    // Input
    // ---------------------------------------------------------------------

    // The host owns SDL_PollEvent() and forwards only events intended for
    // this emulator instance.
    void HandleEvent(const SDL_Event& event) noexcept;

    void ClearInput() noexcept;

    // Current local keyboard state. The same state is forwarded to the
    // Game Boy Joypad device.
    [[nodiscard]] const InputState& Input() const noexcept;

    // ---------------------------------------------------------------------
    // State
    // ---------------------------------------------------------------------

    [[nodiscard]] bool HasRenderer() const noexcept;
    [[nodiscard]] bool IsAudioInitialized() const noexcept;

private:
    static constexpr std::uint32_t T_CYCLES_PER_FRAME =
        456u * 154u;

    using VideoBuffer = std::array<
        std::uint32_t,
        PixelLink::GameBoy::PPU::FRAMEBUFFER_SIZE
    >;

    [[nodiscard]] bool CreateTexture() noexcept;
    [[nodiscard]] bool UpdateVideoFromCore() noexcept;

    void ConvertFramebuffer() noexcept;
    void SetKeyState(int scancode, bool pressed) noexcept;

    PixelLink::GameBoy::GameBoy gameBoy_{};

    // Non-owning. The host owns the renderer.
    SDL_Renderer* renderer_ = nullptr;

    // Owned SDL resources.
    SDL_Texture* texture_ = nullptr;
    SDL_AudioStream* audioStream_ = nullptr;

    int audioChannels_ = 0;

    // Preserves instruction-cycle overshoot across RunFrame() calls.
    std::uint32_t frameCycleRemainder_ = 0;

    VideoBuffer videoBuffer_{};
    InputState inputState_{};
};

} // namespace PixelLink::Frontend
