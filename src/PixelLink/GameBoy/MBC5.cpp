#include "PixelLink/GameBoy/MBC5.hpp"

namespace PixelLink::GameBoy {

MBC5::MBC5(std::vector<uint8_t>& rom)
    : rom_(rom) {
}

auto MBC5::ReadROM(uint16_t address) const -> uint8_t {
    if (address < 0x4000) {
        return rom_[address];
    }

    const auto bank = static_cast<std::size_t>(romBank_);
    const auto offset = bank * 0x4000 + (address - 0x4000);

    if (offset >= rom_.size()) {
        return 0xFF;
    }

    return rom_[offset];
}

auto MBC5::WriteROM(uint16_t address, uint8_t value) -> void {
    if (address <= 0x1FFF) {
        ramEnabled_ = (value & 0x0F) == 0x0A;
        return;
    }

    if (address <= 0x2FFF) {
        romBank_ =
            static_cast<uint16_t>((romBank_ & 0x100) | value);
        return;
    }

    if (address <= 0x3FFF) {
        romBank_ =
            static_cast<uint16_t>(
                (romBank_ & 0xFF) |
                ((value & 0x01) << 8)
            );
    }
}

}
