#pragma once

#include <cstdint>

namespace PixelLink::GameBoy {

// DMG serial port. The 8192 Hz link clock free-runs independently of DIV
// writes. A disconnected internally clocked port receives eight one bits.
class Serial {
public:
    explicit Serial(std::uint16_t clockPhase = 0) : clock_(clockPhase) {}

    [[nodiscard]] std::uint8_t Read(std::uint16_t address) const noexcept;
    void Write(std::uint16_t address, std::uint8_t value) noexcept;
    void Tick(std::uint32_t tCycles) noexcept;
    // Return the outgoing bit and shift an incoming bit on an external edge.
    [[nodiscard]] bool ClockExternalBit(bool incomingBit) noexcept;
    [[nodiscard]] bool ConsumeInterruptRequest() noexcept;

private:
    std::uint16_t clock_ = 0;
    std::uint8_t data_ = 0;
    std::uint8_t control_ = 0;
    std::uint8_t shiftedBits_ = 0;
    bool interruptRequested_ = false;
    void ShiftBit(bool incomingBit) noexcept;
};

} // namespace PixelLink::GameBoy
