#include <PixelLink/Frontend/Emulator.hpp>

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>

namespace PixelLink::Frontend {

namespace {

// Neutral grayscale palette for the four DMG shades.
// The PPU framebuffer stores palette-resolved shade indices 0..3.
constexpr std::array<std::uint32_t, 4> DMG_COLORS{
    0xFFFFFFFFu,
    0xFFAAAAAAu,
    0xFF555555u,
    0xFF000000u,
};

} // namespace

Emulator::~Emulator()
{
    ShutdownAudio();
    ClearRenderer();
}

// -----------------------------------------------------------------------------
// Core
// -----------------------------------------------------------------------------

void Emulator::LoadROM(const std::filesystem::path& path)
{
    gameBoy_.LoadROM(path);
    frameCycleRemainder_ = 0;
    ClearInput();
}

bool Emulator::RunFrame()
{
    while (frameCycleRemainder_ < T_CYCLES_PER_FRAME) {
        const int tCycles = gameBoy_.Step();

        if (tCycles > 0) {
            frameCycleRemainder_ +=
                static_cast<std::uint32_t>(tCycles);
        }
    }

    frameCycleRemainder_ -= T_CYCLES_PER_FRAME;

    // Running the core does not require SDL. If no renderer is attached,
    // the frame is still considered successfully emulated.
    if (renderer_ == nullptr) {
        return true;
    }

    return UpdateVideoFromCore();
}

PixelLink::GameBoy::GameBoy& Emulator::Core() noexcept
{
    return gameBoy_;
}

const PixelLink::GameBoy::GameBoy& Emulator::Core() const noexcept
{
    return gameBoy_;
}

// -----------------------------------------------------------------------------
// Video
// -----------------------------------------------------------------------------

bool Emulator::SetRenderer(SDL_Renderer* renderer) noexcept
{
    if (renderer == nullptr) {
        return false;
    }

    if (renderer_ == renderer && texture_ != nullptr) {
        return true;
    }

    ClearRenderer();
    renderer_ = renderer;

    if (!CreateTexture()) {
        renderer_ = nullptr;
        return false;
    }

    return UpdateVideoFromCore();
}

void Emulator::ClearRenderer() noexcept
{
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    renderer_ = nullptr;
}

bool Emulator::Render(const SDL_FRect& destination) const noexcept
{
    if (renderer_ == nullptr || texture_ == nullptr) {
        return false;
    }

    return SDL_RenderTexture(
        renderer_,
        texture_,
        nullptr,
        &destination
    );
}

bool Emulator::CreateTexture() noexcept
{
    if (renderer_ == nullptr) {
        return false;
    }

    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (texture_ == nullptr) {
        return false;
    }

    if (!SDL_SetTextureScaleMode(
            texture_,
            SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        return false;
    }

    return true;
}

bool Emulator::UpdateVideoFromCore() noexcept
{
    if (renderer_ == nullptr || texture_ == nullptr) {
        return false;
    }

    ConvertFramebuffer();

    void* destinationPixels = nullptr;
    int destinationPitch = 0;

    if (!SDL_LockTexture(
            texture_,
            nullptr,
            &destinationPixels,
            &destinationPitch)) {
        return false;
    }

    const auto* sourceBytes =
        reinterpret_cast<const std::uint8_t*>(
            videoBuffer_.data()
        );

    auto* destinationBytes =
        static_cast<std::uint8_t*>(
            destinationPixels
        );

    constexpr std::size_t sourcePitch =
        PixelLink::GameBoy::PPU::SCREEN_WIDTH *
        sizeof(std::uint32_t);

    for (std::size_t y = 0;
         y < PixelLink::GameBoy::PPU::SCREEN_HEIGHT;
         ++y) {
        std::memcpy(
            destinationBytes +
                y * static_cast<std::size_t>(destinationPitch),
            sourceBytes + y * sourcePitch,
            sourcePitch
        );
    }

    SDL_UnlockTexture(texture_);
    return true;
}

void Emulator::ConvertFramebuffer() noexcept
{
    const auto& framebuffer =
        gameBoy_.GetPPU().GetFramebuffer();

    for (std::size_t i = 0;
         i < framebuffer.size();
         ++i) {
        const std::uint8_t shade =
            framebuffer[i] & 0x03u;

        videoBuffer_[i] = DMG_COLORS[shade];
    }
}

// -----------------------------------------------------------------------------
// Audio
// -----------------------------------------------------------------------------

bool Emulator::InitializeAudio(
    const AudioConfig& config
) noexcept
{
    if (config.sampleRate <= 0 || config.channels <= 0) {
        return false;
    }

    ShutdownAudio();

    SDL_AudioSpec specification{};
    specification.format = SDL_AUDIO_F32;
    specification.channels = config.channels;
    specification.freq = config.sampleRate;

    audioStream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &specification,
        nullptr,
        nullptr
    );

    if (audioStream_ == nullptr) {
        return false;
    }

    if (!SDL_ResumeAudioStreamDevice(audioStream_)) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
        return false;
    }

    audioChannels_ = config.channels;
    return true;
}

void Emulator::ShutdownAudio() noexcept
{
    if (audioStream_ != nullptr) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }

    audioChannels_ = 0;
}

bool Emulator::PushAudio(
    std::span<const float> interleavedSamples
) noexcept
{
    if (audioStream_ == nullptr || audioChannels_ <= 0) {
        return false;
    }

    if (interleavedSamples.empty()) {
        return true;
    }

    if (interleavedSamples.size() %
            static_cast<std::size_t>(audioChannels_) != 0) {
        return false;
    }

    if (interleavedSamples.size_bytes() >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max()
        )) {
        return false;
    }

    return SDL_PutAudioStreamData(
        audioStream_,
        interleavedSamples.data(),
        static_cast<int>(
            interleavedSamples.size_bytes()
        )
    );
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

void Emulator::HandleEvent(const SDL_Event& event) noexcept
{
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        ClearInput();
        return;
    }

    if (event.type != SDL_EVENT_KEY_DOWN &&
        event.type != SDL_EVENT_KEY_UP) {
        return;
    }

    SetKeyState(
        static_cast<int>(event.key.scancode),
        event.type == SDL_EVENT_KEY_DOWN
    );
}

void Emulator::ClearInput() noexcept
{
    using PixelLink::GameBoy::JoypadButton;

    gameBoy_.SetButton(JoypadButton::A, false);
    gameBoy_.SetButton(JoypadButton::B, false);
    gameBoy_.SetButton(JoypadButton::Start, false);
    gameBoy_.SetButton(JoypadButton::Select, false);
    gameBoy_.SetButton(JoypadButton::Up, false);
    gameBoy_.SetButton(JoypadButton::Down, false);
    gameBoy_.SetButton(JoypadButton::Left, false);
    gameBoy_.SetButton(JoypadButton::Right, false);

    inputState_ = {};
}

const Emulator::InputState& Emulator::Input() const noexcept
{
    return inputState_;
}

void Emulator::SetKeyState(
    const int scancode,
    const bool pressed
) noexcept
{
    using PixelLink::GameBoy::JoypadButton;

    switch (scancode) {
    case SDL_SCANCODE_Z:
        inputState_.a = pressed;
        gameBoy_.SetButton(JoypadButton::A, pressed);
        break;

    case SDL_SCANCODE_X:
        inputState_.b = pressed;
        gameBoy_.SetButton(JoypadButton::B, pressed);
        break;

    case SDL_SCANCODE_RETURN:
        inputState_.start = pressed;
        gameBoy_.SetButton(JoypadButton::Start, pressed);
        break;

    case SDL_SCANCODE_BACKSPACE:
        inputState_.select = pressed;
        gameBoy_.SetButton(JoypadButton::Select, pressed);
        break;

    case SDL_SCANCODE_UP:
        inputState_.up = pressed;
        gameBoy_.SetButton(JoypadButton::Up, pressed);
        break;

    case SDL_SCANCODE_DOWN:
        inputState_.down = pressed;
        gameBoy_.SetButton(JoypadButton::Down, pressed);
        break;

    case SDL_SCANCODE_LEFT:
        inputState_.left = pressed;
        gameBoy_.SetButton(JoypadButton::Left, pressed);
        break;

    case SDL_SCANCODE_RIGHT:
        inputState_.right = pressed;
        gameBoy_.SetButton(JoypadButton::Right, pressed);
        break;

    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------

bool Emulator::HasRenderer() const noexcept
{
    return renderer_ != nullptr;
}

bool Emulator::IsAudioInitialized() const noexcept
{
    return audioStream_ != nullptr;
}

} // namespace PixelLink::Frontend
