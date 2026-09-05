#pragma once

#include <cstdint>

class Timer {
public:
    void tick(uint32_t tCycles);

    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);

    bool consumeInterruptRequest();

private:
    uint16_t systemCounter_ = 0;

    uint8_t tima_ = 0;
    uint8_t tma_ = 0;
    uint8_t tac_ = 0;

    bool interruptRequested_ = false;

    // TIMA stays 0 for one M-cycle after overflowing.
    uint8_t overflowDelay_ = 0;

    bool timerSignal() const;
    void incrementTima();
    void tickOneCycle();
};