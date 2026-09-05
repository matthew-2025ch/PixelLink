#include "Timer.hpp"

#include <stdexcept>

namespace {

    constexpr uint16_t DIV = 0xFF04;
    constexpr uint16_t TIMA = 0xFF05;
    constexpr uint16_t TMA = 0xFF06;
    constexpr uint16_t TAC = 0xFF07;

} // namespace

void Timer::tick(uint32_t tCycles) {
    for (uint32_t i = 0; i < tCycles; ++i) {
        tickOneCycle();
    }
}

void Timer::tickOneCycle() {
    // Handle delayed TIMA reload after overflow.
    if (overflowDelay_ > 0) {
        --overflowDelay_;

        if (overflowDelay_ == 0) {
            tima_ = tma_;
            interruptRequested_ = true;
        }
    }

    const bool oldSignal = timerSignal();

    ++systemCounter_;

    const bool newSignal = timerSignal();

    // TIMA increments on the falling edge of the selected timer signal.
    if (oldSignal && !newSignal) {
        incrementTima();
    }
}

bool Timer::timerSignal() const {
    // TAC bit 2 enables TIMA.
    if ((tac_ & 0x04) == 0) {
        return false;
    }

    static constexpr uint8_t clockBits[] = {
        9, // 00: 4096 Hz
        3, // 01: 262144 Hz
        5, // 10: 65536 Hz
        7  // 11: 16384 Hz
    };

    const uint8_t bit = clockBits[tac_ & 0x03];

    return ((systemCounter_ >> bit) & 0x01) != 0;
}

void Timer::incrementTima() {
    if (overflowDelay_ > 0) {
        return;
    }

    if (tima_ == 0xFF) {
        tima_ = 0x00;

        // One M-cycle = 4 T-cycles.
        overflowDelay_ = 4;
    }
    else {
        ++tima_;
    }
}

uint8_t Timer::read(uint16_t address) const {
    switch (address) {
    case DIV:
        return static_cast<uint8_t>(systemCounter_ >> 8);

    case TIMA:
        return tima_;

    case TMA:
        return tma_;

    case TAC:
        // Unused TAC bits normally read as 1.
        return tac_ | 0xF8;

    default:
        throw std::runtime_error("Invalid timer read address");
    }
}

void Timer::write(uint16_t address, uint8_t value) {
    switch (address) {
    case DIV: {
        const bool oldSignal = timerSignal();

        systemCounter_ = 0;

        const bool newSignal = timerSignal();

        // Resetting DIV can create a timer falling edge.
        if (oldSignal && !newSignal) {
            incrementTima();
        }

        break;
    }

    case TIMA:
        tima_ = value;

        // Writing TIMA during the overflow delay cancels the reload.
        if (overflowDelay_ > 0) {
            overflowDelay_ = 0;
        }

        break;

    case TMA:
        tma_ = value;
        break;

    case TAC: {
        const bool oldSignal = timerSignal();

        tac_ = value & 0x07;

        const bool newSignal = timerSignal();

        // Changing TAC can also create a falling edge.
        if (oldSignal && !newSignal) {
            incrementTima();
        }

        break;
    }

    default:
        throw std::runtime_error("Invalid timer write address");
    }
}

bool Timer::consumeInterruptRequest() {
    if (!interruptRequested_) {
        return false;
    }

    interruptRequested_ = false;
    return true;
}