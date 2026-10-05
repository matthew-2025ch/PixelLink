#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace PixelLink::GameBoy {

// DMG audio generator. Produces interleaved floating-point stereo PCM.
class APU {
public:
    static constexpr std::uint32_t CLOCK_RATE = 4'194'304;
    static constexpr std::uint32_t SAMPLE_RATE = 48'000;

    APU() = default;

    auto Reset() noexcept -> void;
    [[nodiscard]] auto Read(std::uint16_t address) const noexcept -> std::uint8_t;
    auto Write(std::uint16_t address, std::uint8_t value) noexcept -> void;
    auto Tick(std::uint32_t tCycles) -> void;
    // The DMG frame sequencer is clocked by falling edges of DIV bit 4.
    // Standalone APUs use a local divider; GameBoy connects the real Timer.
    auto SetExternalDividerClock(bool enabled) noexcept -> void;
    auto ClockDivider() noexcept -> void;

    // SDL may request a different PCM rate; the Game Boy clock is unchanged.
    [[nodiscard]] auto SetSampleRate(std::uint32_t sampleRate) noexcept -> bool;
    auto SetSampleCaptureEnabled(bool enabled) noexcept -> void;
    [[nodiscard]] auto TakeSamples() -> std::vector<float>;

private:
    struct PulseChannel {
        std::array<std::uint8_t, 5> registers{};
        bool enabled = false;
        std::uint8_t length = 0;
        std::uint8_t volume = 0;
        std::uint8_t envelopeTimer = 0;
        std::uint8_t position = 0;
        std::uint16_t timer = 0;
        bool dutyStarted = false;
        bool firstDuty = true;
        bool envelopeRunning = false;
    };

    struct WaveChannel {
        std::array<std::uint8_t, 5> registers{};
        bool enabled = false;
        std::uint16_t length = 0;
        std::uint16_t timer = 0;
        std::uint8_t position = 0;
        std::uint8_t sampleBuffer = 0;
        std::uint8_t accessCycles = 0;
    };

    struct NoiseChannel {
        std::array<std::uint8_t, 4> registers{};
        bool enabled = false;
        std::uint8_t length = 0;
        std::uint8_t volume = 0;
        std::uint8_t envelopeTimer = 0;
        std::uint16_t lfsr = 0x7FFF;
        std::uint32_t timer = 0;
        bool envelopeRunning = false;
    };

    bool powered_ = true;
    bool captureEnabled_ = false;
    PulseChannel pulse1_{};
    PulseChannel pulse2_{};
    WaveChannel wave_{};
    NoiseChannel noise_{};
    std::array<std::uint8_t, 16> waveRAM_{};

    std::uint8_t nr50_ = 0x77;
    std::uint8_t nr51_ = 0xF3;
    std::uint8_t frameStep_ = 0;
    std::uint16_t frameTimer_ = 0;
    bool externalDividerClock_ = false;
    std::uint16_t sweepShadow_ = 0;
    std::uint8_t sweepTimer_ = 0;
    bool sweepEnabled_ = false;
    bool sweepNegated_ = false;
    std::uint32_t sampleRate_ = SAMPLE_RATE;
    std::uint32_t sampleAccumulator_ = 0;
    float highPassLeft_ = 0.0f;
    float highPassRight_ = 0.0f;
    float highPassFactor_ = 0.996337f;
    std::vector<float> samples_;

    [[nodiscard]] static auto PulsePeriod(const PulseChannel& channel) noexcept
        -> std::uint16_t;
    [[nodiscard]] auto WavePeriod() const noexcept -> std::uint16_t;
    [[nodiscard]] auto NoisePeriod() const noexcept -> std::uint32_t;
    [[nodiscard]] static auto PulseLevel(const PulseChannel& channel) noexcept -> float;
    [[nodiscard]] auto WaveLevel() const noexcept -> float;
    [[nodiscard]] auto NoiseLevel() const noexcept -> float;

    auto TriggerPulse(PulseChannel& channel, bool withSweep) noexcept -> void;
    auto TriggerWave() noexcept -> void;
    auto TriggerNoise() noexcept -> void;
    auto ClockEnvelope(PulseChannel& channel) noexcept -> void;
    auto ClockNoiseEnvelope() noexcept -> void;
    auto ClockSweep() noexcept -> void;
    [[nodiscard]] auto SweptPeriod() noexcept -> std::uint16_t;
    template<class Channel>
    auto WriteControl(Channel& channel, std::uint8_t oldControl,
                      std::uint8_t newControl) noexcept -> void;
    auto StepFrameSequencer() noexcept -> void;
    auto EmitSample() -> void;
};

} // namespace PixelLink::GameBoy
