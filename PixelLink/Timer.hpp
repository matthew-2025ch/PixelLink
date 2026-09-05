#pragma once

#include <cstdint>

namespace PixelLink::GameBoy {

class Timer {
public:
    void Tick(uint32_t tCycles);

    uint8_t Read(uint16_t address) const;
    void Write(uint16_t address, uint8_t value);

    bool ConsumeInterruptRequest();

private:
    uint16_t systemCounter_ = 0;

    uint8_t tima_ = 0;
    uint8_t tma_ = 0;
    uint8_t tac_ = 0;

    bool interruptRequested_ = false;

    // TIMA stays 0 for one M-cycle after overflowing.
    uint8_t overflowDelay_ = 0;

    bool TimerSignal() const;
    void IncrementTima();
    void TickOneCycle();
};

} // namespace PixelLink::GameBoy