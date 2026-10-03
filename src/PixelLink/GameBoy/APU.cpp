#include <PixelLink/GameBoy/APU.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace PixelLink::GameBoy {

namespace {
constexpr std::array<std::uint8_t, 4> DUTY_PATTERNS{
    0x01, 0x81, 0x87, 0x7E
};
constexpr std::uint16_t FRAME_SEQUENCER_PERIOD = 8'192;

auto EnvelopeVolume(const std::uint8_t volume) noexcept -> float {
    return static_cast<float>(volume) / 15.0f * 0.25f;
}
} // namespace

auto APU::Reset() noexcept -> void {
    powered_ = true;
    pulse1_ = {};
    pulse2_ = {};
    wave_ = {};
    noise_ = {};
    waveRAM_.fill(0);
    nr50_ = 0x77;
    nr51_ = 0xF3;
    frameStep_ = 0;
    frameTimer_ = 0;
    sweepShadow_ = 0;
    sweepTimer_ = 0;
    sweepEnabled_ = false;
    sampleAccumulator_ = 0;
    highPassLeft_ = highPassRight_ = 0.0f;
    samples_.clear();
}

auto APU::Read(const std::uint16_t address) const noexcept -> std::uint8_t {
    if (0xFF30 <= address && address <= 0xFF3F) {
        return waveRAM_[address - 0xFF30];
    }

    switch (address) {
    case 0xFF10: return static_cast<std::uint8_t>(pulse1_.registers[0] | 0x80);
    case 0xFF11: return static_cast<std::uint8_t>(pulse1_.registers[1] | 0x3F);
    case 0xFF12: return pulse1_.registers[2];
    case 0xFF13: return 0xFF;
    case 0xFF14: return static_cast<std::uint8_t>((pulse1_.registers[4] & 0x40) | 0xBF);
    case 0xFF16: return static_cast<std::uint8_t>(pulse2_.registers[1] | 0x3F);
    case 0xFF17: return pulse2_.registers[2];
    case 0xFF18: return 0xFF;
    case 0xFF19: return static_cast<std::uint8_t>((pulse2_.registers[4] & 0x40) | 0xBF);
    case 0xFF1A: return static_cast<std::uint8_t>(wave_.registers[0] | 0x7F);
    case 0xFF1B: return 0xFF;
    case 0xFF1C: return static_cast<std::uint8_t>(wave_.registers[2] | 0x9F);
    case 0xFF1D: return 0xFF;
    case 0xFF1E: return static_cast<std::uint8_t>((wave_.registers[4] & 0x40) | 0xBF);
    case 0xFF20: return 0xFF;
    case 0xFF21: return noise_.registers[1];
    case 0xFF22: return noise_.registers[2];
    case 0xFF23: return static_cast<std::uint8_t>((noise_.registers[3] & 0x40) | 0xBF);
    case 0xFF24: return nr50_;
    case 0xFF25: return nr51_;
    case 0xFF26:
        return static_cast<std::uint8_t>(
            0x70 | (powered_ ? 0x80 : 0) |
            (pulse1_.enabled ? 0x01 : 0) |
            (pulse2_.enabled ? 0x02 : 0) |
            (wave_.enabled ? 0x04 : 0) |
            (noise_.enabled ? 0x08 : 0));
    default: return 0xFF;
    }
}

auto APU::Write(const std::uint16_t address, const std::uint8_t value) noexcept -> void {
    if (0xFF30 <= address && address <= 0xFF3F) {
        waveRAM_[address - 0xFF30] = value;
        return;
    }

    if (address == 0xFF26) {
        if ((value & 0x80) == 0 && powered_) {
            powered_ = false;
            pulse1_ = {};
            pulse2_ = {};
            wave_ = {};
            noise_ = {};
            nr50_ = nr51_ = 0;
            sweepEnabled_ = false;
        } else if ((value & 0x80) != 0) {
            powered_ = true;
        }
        return;
    }

    if (!powered_) {
        return;
    }

    if (0xFF10 <= address && address <= 0xFF14) {
        const auto index = address - 0xFF10;
        pulse1_.registers[index] = value;
        if (index == 1) {
            pulse1_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 2 && (value & 0xF8) == 0) {
            pulse1_.enabled = false;
        } else if (index == 4 && (value & 0x80) != 0) {
            TriggerPulse(pulse1_, true);
        }
        return;
    }

    if (0xFF16 <= address && address <= 0xFF19) {
        const auto index = address - 0xFF15;
        pulse2_.registers[index] = value;
        if (index == 1) {
            pulse2_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 2 && (value & 0xF8) == 0) {
            pulse2_.enabled = false;
        } else if (index == 4 && (value & 0x80) != 0) {
            TriggerPulse(pulse2_, false);
        }
        return;
    }

    if (0xFF1A <= address && address <= 0xFF1E) {
        const auto index = address - 0xFF1A;
        wave_.registers[index] = value;
        if (index == 0 && (value & 0x80) == 0) {
            wave_.enabled = false;
        } else if (index == 1) {
            wave_.length = static_cast<std::uint16_t>(256 - value);
        } else if (index == 4 && (value & 0x80) != 0) {
            TriggerWave();
        }
        return;
    }

    if (0xFF20 <= address && address <= 0xFF23) {
        const auto index = address - 0xFF20;
        noise_.registers[index] = value;
        if (index == 0) {
            noise_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 1 && (value & 0xF8) == 0) {
            noise_.enabled = false;
        } else if (index == 3 && (value & 0x80) != 0) {
            TriggerNoise();
        }
        return;
    }

    if (address == 0xFF24) {
        nr50_ = value;
    } else if (address == 0xFF25) {
        nr51_ = value;
    }
}

auto APU::PulsePeriod(const PulseChannel& channel) noexcept -> std::uint16_t {
    const auto frequency = static_cast<std::uint16_t>(
        channel.registers[3] | ((channel.registers[4] & 7) << 8));
    return static_cast<std::uint16_t>(4 * (2048 - frequency));
}

auto APU::WavePeriod() const noexcept -> std::uint16_t {
    const auto frequency = static_cast<std::uint16_t>(
        wave_.registers[3] | ((wave_.registers[4] & 7) << 8));
    return static_cast<std::uint16_t>(2 * (2048 - frequency));
}

auto APU::NoisePeriod() const noexcept -> std::uint32_t {
    const auto control = noise_.registers[2];
    const auto shift = control >> 4;
    if (shift >= 14) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    const auto divider = control & 7;
    return (divider == 0 ? 8u : 16u * divider) << shift;
}

auto APU::TriggerPulse(PulseChannel& channel, const bool withSweep) noexcept -> void {
    if (channel.length == 0) {
        channel.length = 64;
    }
    channel.enabled = (channel.registers[2] & 0xF8) != 0;
    channel.volume = channel.registers[2] >> 4;
    channel.envelopeTimer = channel.registers[2] & 7;
    channel.timer = PulsePeriod(channel);

    if (withSweep) {
        sweepShadow_ = static_cast<std::uint16_t>(
            channel.registers[3] | ((channel.registers[4] & 7) << 8));
        const auto pace = static_cast<std::uint8_t>((channel.registers[0] >> 4) & 7);
        sweepTimer_ = pace == 0 ? 8 : pace;
        sweepEnabled_ = pace != 0 || (channel.registers[0] & 7) != 0;
        if ((channel.registers[0] & 7) != 0 && SweptPeriod() > 2047) {
            channel.enabled = false;
        }
    }
}

auto APU::TriggerWave() noexcept -> void {
    if (wave_.length == 0) {
        wave_.length = 256;
    }
    wave_.enabled = (wave_.registers[0] & 0x80) != 0;
    wave_.timer = WavePeriod();
    wave_.position = 0;
}

auto APU::TriggerNoise() noexcept -> void {
    if (noise_.length == 0) {
        noise_.length = 64;
    }
    noise_.enabled = (noise_.registers[1] & 0xF8) != 0;
    noise_.volume = noise_.registers[1] >> 4;
    noise_.envelopeTimer = noise_.registers[1] & 7;
    noise_.lfsr = 0x7FFF;
    noise_.timer = NoisePeriod();
}

auto APU::ClockEnvelope(PulseChannel& channel) noexcept -> void {
    const auto pace = static_cast<std::uint8_t>(channel.registers[2] & 7);
    if (pace == 0 || channel.envelopeTimer == 0 || --channel.envelopeTimer != 0) {
        return;
    }
    channel.envelopeTimer = pace;
    if ((channel.registers[2] & 8) != 0 && channel.volume < 15) {
        ++channel.volume;
    } else if ((channel.registers[2] & 8) == 0 && channel.volume > 0) {
        --channel.volume;
    }
}

auto APU::ClockNoiseEnvelope() noexcept -> void {
    const auto pace = static_cast<std::uint8_t>(noise_.registers[1] & 7);
    if (pace == 0 || noise_.envelopeTimer == 0 || --noise_.envelopeTimer != 0) {
        return;
    }
    noise_.envelopeTimer = pace;
    if ((noise_.registers[1] & 8) != 0 && noise_.volume < 15) {
        ++noise_.volume;
    } else if ((noise_.registers[1] & 8) == 0 && noise_.volume > 0) {
        --noise_.volume;
    }
}

auto APU::SweptPeriod() const noexcept -> std::uint16_t {
    const auto shift = pulse1_.registers[0] & 7;
    const auto change = sweepShadow_ >> shift;
    return static_cast<std::uint16_t>(
        (pulse1_.registers[0] & 8) != 0
            ? sweepShadow_ - change
            : sweepShadow_ + change);
}

auto APU::ClockSweep() noexcept -> void {
    if (!sweepEnabled_ || sweepTimer_ == 0 || --sweepTimer_ != 0) {
        return;
    }
    const auto pace = static_cast<std::uint8_t>((pulse1_.registers[0] >> 4) & 7);
    sweepTimer_ = pace == 0 ? 8 : pace;
    if (pace == 0 || (pulse1_.registers[0] & 7) == 0) {
        return;
    }
    const auto next = SweptPeriod();
    if (next > 2047) {
        pulse1_.enabled = false;
        return;
    }
    sweepShadow_ = next;
    pulse1_.registers[3] = static_cast<std::uint8_t>(next);
    pulse1_.registers[4] = static_cast<std::uint8_t>(
        (pulse1_.registers[4] & 0xF8) | (next >> 8));
    if (SweptPeriod() > 2047) {
        pulse1_.enabled = false;
    }
}

auto APU::StepFrameSequencer() noexcept -> void {
    if ((frameStep_ & 1) == 0) {
        auto clockLength = [](auto& channel, const std::uint8_t control) {
            if ((control & 0x40) != 0 && channel.length > 0 && --channel.length == 0) {
                channel.enabled = false;
            }
        };
        clockLength(pulse1_, pulse1_.registers[4]);
        clockLength(pulse2_, pulse2_.registers[4]);
        clockLength(wave_, wave_.registers[4]);
        clockLength(noise_, noise_.registers[3]);
    }
    if (frameStep_ == 2 || frameStep_ == 6) {
        ClockSweep();
    }
    if (frameStep_ == 7) {
        ClockEnvelope(pulse1_);
        ClockEnvelope(pulse2_);
        ClockNoiseEnvelope();
    }
    frameStep_ = static_cast<std::uint8_t>((frameStep_ + 1) & 7);
}

auto APU::PulseLevel(const PulseChannel& channel) noexcept -> float {
    if (!channel.enabled) {
        return 0.0f;
    }
    const auto pattern = DUTY_PATTERNS[channel.registers[1] >> 6];
    const bool high = (pattern & (1u << channel.position)) != 0;
    const float amplitude = EnvelopeVolume(channel.volume);
    return high ? amplitude : -amplitude;
}

auto APU::WaveLevel() const noexcept -> float {
    if (!wave_.enabled) {
        return 0.0f;
    }
    const auto level = (wave_.registers[2] >> 5) & 3;
    if (level == 0) {
        return 0.0f;
    }
    const auto packed = waveRAM_[wave_.position / 2];
    const auto nibble = wave_.position % 2 == 0 ? packed >> 4 : packed & 0x0F;
    const auto shifted = nibble >> (level - 1);
    return (static_cast<float>(shifted) / 7.5f - 1.0f) * 0.25f;
}

auto APU::NoiseLevel() const noexcept -> float {
    if (!noise_.enabled) {
        return 0.0f;
    }
    const float amplitude = EnvelopeVolume(noise_.volume);
    return (noise_.lfsr & 1) == 0 ? amplitude : -amplitude;
}

auto APU::EmitSample() -> void {
    if (samples_.size() >= static_cast<std::size_t>(sampleRate_) * 2) {
        samples_.clear(); // Keep a disconnected frontend from growing indefinitely.
    }
    const std::array<float, 4> levels{
        PulseLevel(pulse1_), PulseLevel(pulse2_), WaveLevel(), NoiseLevel()
    };
    float left = 0.0f;
    float right = 0.0f;
    for (std::uint8_t i = 0; i < 4; ++i) {
        if ((nr51_ & (1u << (i + 4))) != 0) {
            left += levels[i];
        }
        if ((nr51_ & (1u << i)) != 0) {
            right += levels[i];
        }
    }
    left *= static_cast<float>(((nr50_ >> 4) & 7) + 1) / 8.0f;
    right *= static_cast<float>((nr50_ & 7) + 1) / 8.0f;

    // The hardware output is AC-coupled. Remove DC before feeding SDL.
    const float filteredLeft = left - highPassLeft_;
    const float filteredRight = right - highPassRight_;
    highPassLeft_ = left - filteredLeft * 0.996f;
    highPassRight_ = right - filteredRight * 0.996f;
    samples_.push_back(filteredLeft);
    samples_.push_back(filteredRight);
}

auto APU::Tick(const std::uint32_t tCycles) -> void {
    for (std::uint32_t i = 0; i < tCycles; ++i) {
        if (powered_) {
            auto clockPulse = [](PulseChannel& channel) {
                if (channel.enabled && channel.timer > 0 && --channel.timer == 0) {
                    channel.timer = PulsePeriod(channel);
                    channel.position = static_cast<std::uint8_t>((channel.position + 1) & 7);
                }
            };
            clockPulse(pulse1_);
            clockPulse(pulse2_);
            if (wave_.enabled && wave_.timer > 0 && --wave_.timer == 0) {
                wave_.timer = WavePeriod();
                wave_.position = static_cast<std::uint8_t>((wave_.position + 1) & 31);
            }
            if (noise_.enabled && noise_.timer > 0 && --noise_.timer == 0) {
                noise_.timer = NoisePeriod();
                const auto feedback = static_cast<std::uint16_t>(
                    (noise_.lfsr ^ (noise_.lfsr >> 1)) & 1);
                noise_.lfsr = static_cast<std::uint16_t>(
                    (noise_.lfsr >> 1) | (feedback << 14));
                if ((noise_.registers[2] & 8) != 0) {
                    noise_.lfsr = static_cast<std::uint16_t>(
                        (noise_.lfsr & ~0x40u) | (feedback << 6));
                }
            }
            if (++frameTimer_ == FRAME_SEQUENCER_PERIOD) {
                frameTimer_ = 0;
                StepFrameSequencer();
            }
        }

        if (captureEnabled_) {
            sampleAccumulator_ += sampleRate_;
            if (sampleAccumulator_ >= CLOCK_RATE) {
                sampleAccumulator_ -= CLOCK_RATE;
                EmitSample();
            }
        }
    }
}

auto APU::SetSampleRate(const std::uint32_t sampleRate) noexcept -> bool {
    if (sampleRate < 8'000 || sampleRate > 192'000) {
        return false;
    }
    sampleRate_ = sampleRate;
    sampleAccumulator_ = 0;
    samples_.clear();
    return true;
}

auto APU::SetSampleCaptureEnabled(const bool enabled) noexcept -> void {
    captureEnabled_ = enabled;
    sampleAccumulator_ = 0;
    samples_.clear();
}

auto APU::TakeSamples() -> std::vector<float> {
    return std::exchange(samples_, {});
}

} // namespace PixelLink::GameBoy
