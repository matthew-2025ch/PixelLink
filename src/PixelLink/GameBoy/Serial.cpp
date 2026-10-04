#include <PixelLink/GameBoy/Serial.hpp>

namespace PixelLink::GameBoy {

std::uint8_t Serial::Read(const std::uint16_t address) const noexcept {
    return address == 0xFF01 ? data_ : static_cast<std::uint8_t>(control_ | 0x7E);
}

void Serial::Write(const std::uint16_t address, const std::uint8_t value) noexcept {
    if (address == 0xFF01) {
        data_ = value;
    } else {
        control_ = value & 0x81;
        shiftedBits_ = 0;
    }
}

void Serial::Tick(const std::uint32_t tCycles) noexcept {
    for (std::uint32_t cycle = 0; cycle < tCycles; ++cycle) {
        ++clock_;
        if ((clock_ & 0x1FF) == 0 && (control_ & 0x81) == 0x81) {
            ShiftBit(true);
        }
    }
}

bool Serial::ClockExternalBit(const bool incomingBit) noexcept {
    const bool outgoingBit = (data_ & 0x80) != 0;
    if ((control_ & 0x81) == 0x80) {
        ShiftBit(incomingBit);
    }
    return outgoingBit;
}

void Serial::ShiftBit(const bool incomingBit) noexcept {
    data_ = static_cast<std::uint8_t>((data_ << 1) | (incomingBit ? 1 : 0));
    if (++shiftedBits_ == 8) {
        control_ &= 0x01;
        shiftedBits_ = 0;
        interruptRequested_ = true;
    }
}

bool Serial::ConsumeInterruptRequest() noexcept {
    const bool requested = interruptRequested_;
    interruptRequested_ = false;
    return requested;
}

} // namespace PixelLink::GameBoy
