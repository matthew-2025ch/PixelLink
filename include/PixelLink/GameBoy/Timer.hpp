#pragma once

#include <cstdint>

namespace PixelLink::GameBoy {

class APU;

class Timer {
public:
    void Tick(uint32_t tCycles);

    uint8_t Read(uint16_t address) const;
    void Write(uint16_t address, uint8_t value);

    bool ConsumeInterruptRequest();
    void AttachAPU(APU& apu) noexcept;

private:
    uint16_t systemCounter_ = 0;
    APU* apu_ = nullptr; // Non-owning; GameBoy owns both devices.

    uint8_t tima_ = 0;
    uint8_t tma_ = 0;
    uint8_t tac_ = 0;

    bool interruptRequested_ = false;

    // TIMA stays 0 for one M-cycle after overflowing.
    uint8_t overflowDelay_ = 0;

    // During the reload M-cycle, TIMA writes are ignored and TMA writes
    // also update TIMA. This is distinct from the cancellable overflow delay.
    uint8_t reloadCyclesRemaining_ = 0;

    bool TimerSignal() const;
    void IncrementTima();
    void TickOneCycle();
};

} // namespace PixelLink::GameBoy
