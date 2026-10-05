#include <algorithm>
#include <cstdint>
#include <cmath>

#include <PixelLink/GameBoy/APU.hpp>
#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Test::GameBoy::APUTest {
namespace {

void testPowerAndTrigger() {
    PixelLink::GameBoy::GameBoy gameBoy;
    auto& bus = gameBoy.GetBus();

    CHECK(bus.Read(0xFF26) == 0xF0);
    bus.Write(0xFF17, 0xF0);
    bus.Write(0xFF19, 0x80);
    CHECK((bus.Read(0xFF26) & 0x02) != 0);

    bus.Write(0xFF26, 0);
    CHECK(bus.Read(0xFF26) == 0x70);
    CHECK(bus.Read(0xFF17) == 0);
    bus.Write(0xFF17, 0xF0);
    CHECK(bus.Read(0xFF17) == 0);

    bus.Write(0xFF26, 0x80);
    CHECK(bus.Read(0xFF26) == 0xF0);
    bus.Write(0xFF19, 0x80);
    CHECK((bus.Read(0xFF26) & 0x02) == 0); // DAC is still off.
}

void testPulseAndStereoRouting() {
    PixelLink::GameBoy::GameBoy gameBoy;
    auto& bus = gameBoy.GetBus();
    auto& apu = gameBoy.GetAPU();
    apu.SetSampleCaptureEnabled(true);

    bus.Write(0xFF25, 0x20); // CH2 to left only.
    bus.Write(0xFF16, 0x80); // 50% duty.
    bus.Write(0xFF17, 0xF0); // Maximum constant volume.
    bus.Write(0xFF18, 0x00);
    bus.Write(0xFF19, 0x87); // Trigger at period 0x700.
    apu.Tick(8'192);

    const auto samples = apu.TakeSamples();
    CHECK(samples.size() == 186); // 93 stereo frames at 48 kHz.
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < samples.size(); i += 2) {
        positive |= samples[i] > 0;
        negative |= samples[i] < 0;
        CHECK(samples[i + 1] == 0.0f);
    }
    CHECK(positive);
    CHECK(negative);
    CHECK(apu.TakeSamples().empty());
}

void testLengthAndCoreClock() {
    PixelLink::GameBoy::GameBoy gameBoy;
    auto& bus = gameBoy.GetBus();
    auto& apu = gameBoy.GetAPU();
    apu.SetSampleCaptureEnabled(true);

    bus.Write(0xFF16, 0x3F); // One length tick remains.
    bus.Write(0xFF17, 0xF0);
    bus.Write(0xFF19, 0xC0); // Trigger with length enabled.
    CHECK((bus.Read(0xFF26) & 0x02) != 0);

    // GameBoy::Step advances the APU along with the other devices.
    for (int i = 0; i < 2'048; ++i) {
        CHECK(gameBoy.Step() == 4);
    }
    CHECK((bus.Read(0xFF26) & 0x02) == 0);
    CHECK(!apu.TakeSamples().empty());
}

void testVolumeEnvelope() {
    PixelLink::GameBoy::APU apu;
    apu.SetSampleCaptureEnabled(true);
    apu.Write(0xFF25, 0x20);
    apu.Write(0xFF16, 0x80);
    apu.Write(0xFF17, 0xF1); // Decrease from volume 15 every 64 Hz tick.
    apu.Write(0xFF18, 0xFF);
    apu.Write(0xFF19, 0x87);

    apu.Tick(8'192);
    const auto before = apu.TakeSamples();
    float firstPeak = -1.0f;
    float firstLow = 1.0f;
    for (std::size_t i = 0; i < before.size(); i += 2) {
        firstPeak = std::max(firstPeak, before[i]);
        firstLow = std::min(firstLow, before[i]);
    }
    CHECK(firstPeak > 0.0f);

    apu.Tick(7 * 8'192); // Frame sequencer step 7 clocks the envelope.
    (void)apu.TakeSamples();
    apu.Tick(8'192);
    const auto after = apu.TakeSamples();
    float laterPeak = -1.0f;
    float laterLow = 1.0f;
    for (std::size_t i = 0; i < after.size(); i += 2) {
        laterPeak = std::max(laterPeak, after[i]);
        laterLow = std::min(laterLow, after[i]);
    }
    // DAC offset changes as volume changes; peak-to-peak amplitude tests the
    // envelope independently of the high-pass capacitor's current charge.
    CHECK(laterPeak - laterLow < firstPeak - firstLow);
}

void testPulse1Sweep() {
    PixelLink::GameBoy::APU apu;
    apu.Write(0xFF10, 0x11); // Sweep every 128 Hz tick, add half the period.
    apu.Write(0xFF11, 0x80);
    apu.Write(0xFF12, 0xF0);
    apu.Write(0xFF13, 0x00);
    apu.Write(0xFF14, 0x85); // Initial period 0x500.
    CHECK((apu.Read(0xFF26) & 0x01) != 0);
    apu.Tick(3 * 8'192);
    // 0x500 -> 0x780, then the required overflow check disables CH1.
    CHECK((apu.Read(0xFF26) & 0x01) == 0);
}

void testPulse1Output() {
    PixelLink::GameBoy::APU apu;
    apu.SetSampleCaptureEnabled(true);
    apu.Write(0xFF25, 0x10); // CH1 left only.
    apu.Write(0xFF10, 0);
    apu.Write(0xFF11, 0x80);
    apu.Write(0xFF12, 0xF0);
    apu.Write(0xFF13, 0);
    apu.Write(0xFF14, 0x87);
    apu.Tick(8'192);

    const auto samples = apu.TakeSamples();
    CHECK(std::any_of(samples.begin(), samples.end(), [](float sample) {
        return sample > 0.0f;
    }));
    for (std::size_t i = 1; i < samples.size(); i += 2) {
        CHECK(samples[i] == 0.0f);
    }
}

void testWaveChannelAndRAM() {
    PixelLink::GameBoy::APU apu;
    apu.SetSampleCaptureEnabled(true);
    apu.Write(0xFF25, 0x40); // CH3 left only.
    // CH3 skips nibble zero on trigger; repeat the waveform so both levels
    // occur before the first complete 32-sample loop.
    for (int i = 0; i < 16; ++i) apu.Write(0xFF30 + i, 0xF0);
    CHECK(apu.Read(0xFF30) == 0xF0);
    apu.Write(0xFF1A, 0x80); // DAC on.
    apu.Write(0xFF1C, 0x20); // Full output level.
    apu.Write(0xFF1D, 0x00);
    apu.Write(0xFF1E, 0x87);
    CHECK((apu.Read(0xFF26) & 0x04) != 0);
    apu.Tick(8'192);

    const auto samples = apu.TakeSamples();
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < samples.size(); i += 2) {
        positive |= samples[i] > 0;
        negative |= samples[i] < 0;
        CHECK(samples[i + 1] == 0.0f);
    }
    CHECK(positive && negative);

    apu.Write(0xFF26, 0);
    CHECK(apu.Read(0xFF30) == 0xF0); // Power-off preserves wave RAM.
    CHECK((apu.Read(0xFF26) & 0x04) == 0);
}

void testNoiseChannel() {
    PixelLink::GameBoy::APU apu;
    apu.SetSampleCaptureEnabled(true);
    apu.Write(0xFF25, 0x80); // CH4 left only.
    apu.Write(0xFF21, 0xF0);
    apu.Write(0xFF22, 0x01);
    apu.Write(0xFF23, 0x80);
    CHECK((apu.Read(0xFF26) & 0x08) != 0);
    apu.Tick(8'192);

    const auto samples = apu.TakeSamples();
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < samples.size(); i += 2) {
        positive |= samples[i] > 0;
        negative |= samples[i] < 0;
        CHECK(samples[i + 1] == 0.0f);
    }
    CHECK(positive && negative);

    apu.Write(0xFF21, 0); // Disabling the DAC silences the channel.
    CHECK((apu.Read(0xFF26) & 0x08) == 0);
}

void testWaveLengthAndOutputLevel() {
    PixelLink::GameBoy::APU apu;
    apu.SetSampleCaptureEnabled(true);
    apu.Write(0xFF25, 0x40);
    apu.Write(0xFF30, 0xF0);
    apu.Write(0xFF1A, 0x80);
    apu.Write(0xFF1B, 0xFF); // One length tick.
    apu.Write(0xFF1C, 0); // Mute the wave output.
    apu.Write(0xFF1E, 0xC0);
    apu.Tick(8'192);
    const auto muted = apu.TakeSamples();
    // Muting the digital wave still leaves its DAC on: a DC transient is
    // expected, then the high-pass filter removes it.
    CHECK(!muted.empty());
    CHECK(std::all_of(muted.begin(), muted.end(), [](float sample) {
        return std::isfinite(sample) && std::abs(sample) <= 1.0f;
    }));
    apu.Write(0xFF1A, 0);
    apu.Tick(8'192);
    const auto silent = apu.TakeSamples();
    CHECK(std::all_of(silent.begin(), silent.end(), [](float sample) { return sample == 0; }));
    CHECK((apu.Read(0xFF26) & 0x04) == 0); // Length expired.
}

void testConfigurableSampleRate() {
    PixelLink::GameBoy::APU apu;
    CHECK(!apu.SetSampleRate(0));
    CHECK(apu.SetSampleRate(24'000));
    apu.SetSampleCaptureEnabled(true);
    apu.Tick(8'192);
    CHECK(apu.TakeSamples().size() == 92); // 46 stereo frames.
}

void testDividerFallingEdge() {
    PixelLink::GameBoy::GameBoy gb;
    auto& bus = gb.GetBus();
    auto& timer = gb.GetTimer();
    bus.Write(0xFF16, 0x3F);
    bus.Write(0xFF17, 0xF0);
    bus.Write(0xFF19, 0xC0);
    timer.Tick(4'095);
    bus.Write(0xFF04, 0); // DIV bit 4 low: no APU edge.
    CHECK((bus.Read(0xFF26) & 2) != 0);
    timer.Tick(4'096);
    bus.Write(0xFF04, 0); // DIV bit 4 high: immediate length clock.
    CHECK((bus.Read(0xFF26) & 2) == 0);
    bus.Write(0xFF16, 0x3E);
    bus.Write(0xFF19, 0xC0);
    timer.Tick(8'192); // Odd sequencer step, no length clock.
    CHECK((bus.Read(0xFF26) & 2) != 0);
    timer.Tick(24'576); // Steps 2 and 4 each decrement length once.
    CHECK((bus.Read(0xFF26) & 2) == 0);
}

void testExtraLengthClock() {
    // Cover the same NRx4 behavior in all four channels.
    for (int channel = 0; channel < 4; ++channel) {
        PixelLink::GameBoy::APU apu;
        const std::uint16_t length[] = {0xFF11, 0xFF16, 0xFF1B, 0xFF20};
        const std::uint16_t dac[] = {0xFF12, 0xFF17, 0xFF1A, 0xFF21};
        const std::uint16_t control[] = {0xFF14, 0xFF19, 0xFF1E, 0xFF23};
        apu.Write(dac[channel], channel == 2 ? 0x80 : 0xF0);
        apu.Write(length[channel], channel == 2 ? 255 : 63);
        apu.Write(control[channel], 0x80); // Disabled length counter.
        apu.ClockDivider(); // Next step is odd.
        apu.Write(control[channel], 0x40);
        CHECK((apu.Read(0xFF26) & (1 << channel)) == 0);
        apu.Write(control[channel], 0xC0); // Zero length reloads to max-1.
        CHECK((apu.Read(0xFF26) & (1 << channel)) != 0);
        const int clocks = channel == 2 ? 255 : 63;
        for (int i = 0; i < clocks - 1; ++i) {
            apu.ClockDivider();
            apu.ClockDivider();
        }
        CHECK((apu.Read(0xFF26) & (1 << channel)) != 0);
        apu.ClockDivider();
        apu.ClockDivider();
        CHECK((apu.Read(0xFF26) & (1 << channel)) == 0);
    }
}

void testPowerOffLengthAndPhase() {
    PixelLink::GameBoy::APU apu;
    apu.Write(0xFF26, 0);
    apu.Write(0xFF16, 0xFF); // DMG accepts length, ignores duty while off.
    CHECK(apu.Read(0xFF16) == 0x3F);
    apu.Tick(4'096); // Divider keeps running even with APU powered off.
    apu.Write(0xFF26, 0x80);
    apu.Write(0xFF17, 0xF0);
    apu.Write(0xFF19, 0xC0);
    apu.Tick(4'095);
    CHECK((apu.Read(0xFF26) & 2) != 0);
    apu.Tick(1);
    CHECK((apu.Read(0xFF26) & 2) == 0);
}

void testSweepNegateAndZeroShift() {
    PixelLink::GameBoy::APU apu;
    apu.Write(0xFF10, 0x19);
    apu.Write(0xFF12, 0xF0);
    apu.Write(0xFF14, 0x85);
    CHECK((apu.Read(0xFF26) & 1) != 0);
    apu.Write(0xFF10, 0x11); // Clear negate after trigger's subtraction.
    CHECK((apu.Read(0xFF26) & 1) == 0);
    apu.Write(0xFF10, 0x10); // Shift zero still performs overflow check.
    apu.Write(0xFF14, 0x85);
    CHECK((apu.Read(0xFF26) & 1) != 0);
    apu.Tick(3 * 8'192);
    CHECK((apu.Read(0xFF26) & 1) == 0);
}

void testWaveRAMWindowAndCorruption() {
    PixelLink::GameBoy::APU apu;
    for (int i = 0; i < 16; ++i) apu.Write(0xFF30 + i, 0x10 + i);
    apu.Write(0xFF1A, 0x80);
    apu.Write(0xFF1D, 0xFC); // Eight cycles per sample.
    apu.Write(0xFF1E, 0x87);
    CHECK(apu.Read(0xFF30) == 0xFF);
    apu.Write(0xFF30, 0xEE); // Blocked outside fetch window.
    apu.Tick(14);
    CHECK(apu.Read(0xFF3F) == 0x10); // Addresses alias current byte.
    apu.Write(0xFF3F, 0xA1);
    CHECK(apu.Read(0xFF30) == 0xA1);
    apu.Tick(2);
    CHECK(apu.Read(0xFF30) == 0xFF);
    apu.Tick(6 + 16 * 8); // Sample index 18, byte 9.
    CHECK(apu.Read(0xFF30) == 0x19);
    apu.Tick(6); // Upcoming byte-9 fetch: two cycles remain.
    CHECK(apu.Read(0xFF30) == 0xFF); // CPU access and corruption windows differ.
    apu.Write(0xFF1E, 0x87);
    apu.Write(0xFF1A, 0);
    for (int i = 0; i < 4; ++i) CHECK(apu.Read(0xFF30 + i) == 0x18 + i);
    CHECK(apu.Read(0xFF34) == 0x14);
}

} // namespace

void run() {
    Test::run("APU / power and CH2 trigger", testPowerAndTrigger);
    Test::run("APU / CH2 pulse and stereo routing", testPulseAndStereoRouting);
    Test::run("APU / CH2 length and core clock", testLengthAndCoreClock);
    Test::run("APU / CH2 volume envelope", testVolumeEnvelope);
    Test::run("APU / CH1 sweep", testPulse1Sweep);
    Test::run("APU / CH1 output", testPulse1Output);
    Test::run("APU / CH3 wave RAM and output", testWaveChannelAndRAM);
    Test::run("APU / CH3 length and level", testWaveLengthAndOutputLevel);
    Test::run("APU / CH4 noise and DAC", testNoiseChannel);
    Test::run("APU / sample rate", testConfigurableSampleRate);
    Test::run("APU / real DIV edges and DIV writes", testDividerFallingEdge);
    Test::run("APU / extra length clock and trigger reload", testExtraLengthClock);
    Test::run("APU / DMG power-off length and divider phase", testPowerOffLengthAndPhase);
    Test::run("APU / sweep negate and zero shift", testSweepNegateAndZeroShift);
    Test::run("APU / wave RAM access window and retrigger corruption", testWaveRAMWindowAndCorruption);
}

} // namespace PixelLink::Test::GameBoy::APUTest
