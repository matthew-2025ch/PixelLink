#include <PixelLink/GameBoy/APU.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace PixelLink::GameBoy {

namespace {
constexpr std::array<std::uint8_t, 4> DUTY_PATTERNS{
    0x01, 0x81, 0x87, 0x7E
};
constexpr std::uint16_t FRAME_SEQUENCER_PERIOD = 8'192;

auto DACLevel(const std::uint8_t digital) noexcept -> float {
    return (1.0f - static_cast<float>(digital) / 7.5f) * 0.25f;
}

auto WriteLiveEnvelope(const std::uint8_t oldValue, const std::uint8_t newValue,
                        const bool enabled, const bool running, std::uint8_t& volume) noexcept -> void {
    // The period-zero increment in add mode is consistent across DMG units.
    // Other zombie-envelope variations depend on the physical chip revision.
    if (enabled && running && (oldValue & 0x0F) == 8 && (newValue & 0xF8) != 0) {
        volume = static_cast<std::uint8_t>((volume + 1) & 15);
    }
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
    sweepNegated_ = false;
    sampleAccumulator_ = 0;
    highPassLeft_ = highPassRight_ = 0.0f;
    samples_.clear();
}

auto APU::Read(const std::uint16_t address) const noexcept -> std::uint8_t {
    if (0xFF30 <= address && address <= 0xFF3F) {
        if (wave_.enabled) {
            return wave_.accessCycles != 0 ? waveRAM_[wave_.position / 2] : 0xFF;
        }
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
        if (!wave_.enabled) {
            waveRAM_[address - 0xFF30] = value;
        } else if (wave_.accessCycles != 0) {
            waveRAM_[wave_.position / 2] = value;
        }
        return;
    }

    if (address == 0xFF26) {
        if ((value & 0x80) == 0 && powered_) {
            powered_ = false;
            // DMG length counters survive power-off, unlike CGB counters.
            const auto l1 = pulse1_.length;
            const auto l2 = pulse2_.length;
            const auto lw = wave_.length;
            const auto ln = noise_.length;
            pulse1_ = {};
            pulse2_ = {};
            wave_ = {};
            noise_ = {};
            pulse1_.length = l1;
            pulse2_.length = l2;
            wave_.length = lw;
            noise_.length = ln;
            nr50_ = nr51_ = 0;
            sweepEnabled_ = false;
            sweepNegated_ = false;
            highPassLeft_ = highPassRight_ = 0;
        } else if ((value & 0x80) != 0 && !powered_) {
            powered_ = true;
            frameStep_ = 0;
            wave_.sampleBuffer = 0;
        }
        return;
    }

    if (!powered_) {
        switch (address) {
        case 0xFF11: pulse1_.length = 64 - (value & 0x3F); break;
        case 0xFF16: pulse2_.length = 64 - (value & 0x3F); break;
        case 0xFF1B: wave_.length = 256 - value; break;
        case 0xFF20: noise_.length = 64 - (value & 0x3F); break;
        default: break;
        }
        return;
    }

    if (0xFF10 <= address && address <= 0xFF14) {
        const auto index = address - 0xFF10;
        const auto oldValue = pulse1_.registers[index];
        if (index == 2) WriteLiveEnvelope(oldValue, value, pulse1_.enabled, pulse1_.envelopeRunning, pulse1_.volume);
        pulse1_.registers[index] = value;
        if (index == 0 && (oldValue & 8) != 0 && (value & 8) == 0 && sweepNegated_) {
            pulse1_.enabled = false;
        } else if (index == 1) {
            pulse1_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 2 && (value & 0xF8) == 0) {
            pulse1_.enabled = false;
        } else if (index == 4) {
            WriteControl(pulse1_, oldValue, value);
            if ((value & 0x80) != 0) TriggerPulse(pulse1_, true);
        }
        return;
    }

    if (0xFF16 <= address && address <= 0xFF19) {
        const auto index = address - 0xFF15;
        const auto oldValue = pulse2_.registers[index];
        if (index == 2) WriteLiveEnvelope(oldValue, value, pulse2_.enabled, pulse2_.envelopeRunning, pulse2_.volume);
        pulse2_.registers[index] = value;
        if (index == 1) {
            pulse2_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 2 && (value & 0xF8) == 0) {
            pulse2_.enabled = false;
        } else if (index == 4) {
            WriteControl(pulse2_, oldValue, value);
            if ((value & 0x80) != 0) TriggerPulse(pulse2_, false);
        }
        return;
    }

    if (0xFF1A <= address && address <= 0xFF1E) {
        const auto index = address - 0xFF1A;
        const auto oldValue = wave_.registers[index];
        wave_.registers[index] = value;
        if (index == 0 && (value & 0x80) == 0) {
            wave_.enabled = false;
        } else if (index == 1) {
            wave_.length = static_cast<std::uint16_t>(256 - value);
        } else if (index == 4) {
            WriteControl(wave_, oldValue, value);
            if ((value & 0x80) != 0) TriggerWave();
        }
        return;
    }

    if (0xFF20 <= address && address <= 0xFF23) {
        const auto index = address - 0xFF20;
        const auto oldValue = noise_.registers[index];
        if (index == 1) WriteLiveEnvelope(oldValue, value, noise_.enabled, noise_.envelopeRunning, noise_.volume);
        noise_.registers[index] = value;
        if (index == 0) {
            noise_.length = static_cast<std::uint8_t>(64 - (value & 0x3F));
        } else if (index == 1 && (value & 0xF8) == 0) {
            noise_.enabled = false;
        } else if (index == 3) {
            WriteControl(noise_, oldValue, value);
            if ((value & 0x80) != 0) TriggerNoise();
        }
        return;
    }

    if (address == 0xFF24) {
        nr50_ = value;
    } else if (address == 0xFF25) {
        nr51_ = value;
    }
}

template<class Channel>
auto APU::WriteControl(Channel& channel, const std::uint8_t oldControl,
                       const std::uint8_t newControl) noexcept -> void {
    if ((frameStep_ & 1) != 0 && (oldControl & 0x40) == 0 &&
        (newControl & 0x40) != 0 && channel.length != 0) {
        if (--channel.length == 0 && (newControl & 0x80) == 0) {
            channel.enabled = false;
        }
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
        channel.length = ((frameStep_ & 1) && (channel.registers[4] & 0x40)) ? 63 : 64;
    }
    channel.enabled = (channel.registers[2] & 0xF8) != 0;
    channel.volume = channel.registers[2] >> 4;
    const auto pace = channel.registers[2] & 7;
    channel.envelopeTimer = static_cast<std::uint8_t>((pace == 0 ? 8 : pace) + (frameStep_ == 7 ? 1 : 0));
    channel.envelopeRunning = true;
    channel.timer = static_cast<std::uint16_t>((PulsePeriod(channel) & ~3u) | (channel.timer & 3));
    channel.dutyStarted = true;

    if (withSweep) {
        sweepNegated_ = false;
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
    // DMG corruption happens during the upcoming fetch's two-cycle
    // preparation phase, not in the subsequent CPU RAM access window.
    if (wave_.enabled && wave_.timer == 2) {
        const auto index = ((wave_.position + 1) & 31) / 2;
        if (index < 4) {
            waveRAM_[0] = waveRAM_[index];
        } else {
            const auto start = index & ~3;
            std::array<std::uint8_t, 4> block{};
            std::copy_n(waveRAM_.begin() + start, 4, block.begin());
            std::copy(block.begin(), block.end(), waveRAM_.begin());
        }
    }
    if (wave_.length == 0) {
        wave_.length = ((frameStep_ & 1) && (wave_.registers[4] & 0x40)) ? 255 : 256;
    }
    wave_.enabled = (wave_.registers[0] & 0x80) != 0;
    wave_.timer = static_cast<std::uint16_t>(WavePeriod() + 6);
    wave_.position = 0;
    wave_.accessCycles = 0;
}

auto APU::TriggerNoise() noexcept -> void {
    if (noise_.length == 0) {
        noise_.length = ((frameStep_ & 1) && (noise_.registers[3] & 0x40)) ? 63 : 64;
    }
    noise_.enabled = (noise_.registers[1] & 0xF8) != 0;
    noise_.volume = noise_.registers[1] >> 4;
    const auto pace = noise_.registers[1] & 7;
    noise_.envelopeTimer = static_cast<std::uint8_t>((pace == 0 ? 8 : pace) + (frameStep_ == 7 ? 1 : 0));
    noise_.envelopeRunning = true;
    noise_.lfsr = 0x7FFF;
    noise_.timer = NoisePeriod();
}

auto APU::ClockEnvelope(PulseChannel& channel) noexcept -> void {
    const auto pace = static_cast<std::uint8_t>(channel.registers[2] & 7);
    if (!channel.envelopeRunning || channel.envelopeTimer == 0 || --channel.envelopeTimer != 0) {
        return;
    }
    channel.envelopeTimer = pace == 0 ? 8 : pace;
    if (pace == 0) return;
    if ((channel.registers[2] & 8) != 0 && channel.volume < 15) {
        ++channel.volume;
    } else if ((channel.registers[2] & 8) == 0 && channel.volume > 0) {
        --channel.volume;
    } else {
        channel.envelopeRunning = false;
    }
}

auto APU::ClockNoiseEnvelope() noexcept -> void {
    const auto pace = static_cast<std::uint8_t>(noise_.registers[1] & 7);
    if (!noise_.envelopeRunning || noise_.envelopeTimer == 0 || --noise_.envelopeTimer != 0) {
        return;
    }
    noise_.envelopeTimer = pace == 0 ? 8 : pace;
    if (pace == 0) return;
    if ((noise_.registers[1] & 8) != 0 && noise_.volume < 15) {
        ++noise_.volume;
    } else if ((noise_.registers[1] & 8) == 0 && noise_.volume > 0) {
        --noise_.volume;
    } else {
        noise_.envelopeRunning = false;
    }
}

auto APU::SweptPeriod() noexcept -> std::uint16_t {
    if ((pulse1_.registers[0] & 8) != 0) sweepNegated_ = true;
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
    if (pace == 0) {
        return;
    }
    const auto next = SweptPeriod();
    if (next > 2047) {
        pulse1_.enabled = false;
        return;
    }
    if ((pulse1_.registers[0] & 7) == 0) return;
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
    if ((channel.registers[2] & 0xF8) == 0) {
        return 0.0f;
    }
    const auto pattern = DUTY_PATTERNS[channel.registers[1] >> 6];
    const bool high = !channel.firstDuty && (pattern & (1u << (7 - channel.position))) != 0;
    return DACLevel(channel.enabled && high ? channel.volume : 0);
}

auto APU::WaveLevel() const noexcept -> float {
    if ((wave_.registers[0] & 0x80) == 0) {
        return 0.0f;
    }
    const auto level = (wave_.registers[2] >> 5) & 3;
    const auto packed = wave_.sampleBuffer;
    const auto nibble = wave_.position % 2 == 0 ? packed >> 4 : packed & 0x0F;
    const auto shifted = level == 0 || !wave_.enabled ? 0 : nibble >> (level - 1);
    return DACLevel(static_cast<std::uint8_t>(shifted));
}

auto APU::NoiseLevel() const noexcept -> float {
    if ((noise_.registers[1] & 0xF8) == 0) {
        return 0.0f;
    }
    return DACLevel(noise_.enabled && (noise_.lfsr & 1) == 0 ? noise_.volume : 0);
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
    const bool dacs = powered_ && ((pulse1_.registers[2] & 0xF8) ||
        (pulse2_.registers[2] & 0xF8) || (wave_.registers[0] & 0x80) ||
        (noise_.registers[1] & 0xF8));
    const float filteredLeft = dacs ? left - highPassLeft_ : 0;
    const float filteredRight = dacs ? right - highPassRight_ : 0;
    if (dacs) {
        highPassLeft_ = left - filteredLeft * highPassFactor_;
        highPassRight_ = right - filteredRight * highPassFactor_;
    }
    samples_.push_back(std::clamp(filteredLeft, -1.0f, 1.0f));
    samples_.push_back(std::clamp(filteredRight, -1.0f, 1.0f));
}

auto APU::Tick(const std::uint32_t tCycles) -> void {
    for (std::uint32_t i = 0; i < tCycles; ++i) {
        if (!externalDividerClock_ && ++frameTimer_ == FRAME_SEQUENCER_PERIOD) {
            frameTimer_ = 0;
            ClockDivider();
        }
        if (powered_) {
            auto clockPulse = [](PulseChannel& channel) {
                if (channel.dutyStarted && channel.timer > 0 && --channel.timer == 0) {
                    channel.timer = PulsePeriod(channel);
                    channel.position = static_cast<std::uint8_t>((channel.position + 1) & 7);
                    channel.firstDuty = false;
                }
            };
            clockPulse(pulse1_);
            clockPulse(pulse2_);
            if (wave_.accessCycles != 0) --wave_.accessCycles;
            if (wave_.enabled && wave_.timer > 0 && --wave_.timer == 0) {
                wave_.timer = WavePeriod();
                wave_.position = static_cast<std::uint8_t>((wave_.position + 1) & 31);
                wave_.sampleBuffer = waveRAM_[wave_.position / 2];
                wave_.accessCycles = 2;
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
    highPassFactor_ = static_cast<float>(std::pow(0.999958, static_cast<double>(CLOCK_RATE) / sampleRate));
    sampleAccumulator_ = 0;
    samples_.clear();
    return true;
}

auto APU::SetExternalDividerClock(const bool enabled) noexcept -> void {
    externalDividerClock_ = enabled;
}

auto APU::ClockDivider() noexcept -> void {
    if (powered_) StepFrameSequencer();
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
