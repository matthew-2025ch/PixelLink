#include "PixelLink/GameBoy/Mapper/MBC1.hpp"

namespace PixelLink::GameBoy {

MBC1::MBC1(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
    : rom_(rom), ram_(ram) {
}

auto MBC1::ReadROM(uint16_t address) const -> uint8_t {
    size_t bank = 0;

    if (address >= 0x4000) {
        uint8_t low = romBankLow5_ ? romBankLow5_ : 1;
        bank = (bankHigh2_ << 5) | low;
        address -= 0x4000;
    }

    size_t offset = bank * 0x4000 + address;
    return offset < rom_.size() ? rom_[offset] : 0xFF;
}

auto MBC1::WriteROM(uint16_t address, uint8_t value) -> void {
    if (address <= 0x1FFF) {
        ramEnabled_ = (value & 0x0F) == 0x0A;
    }
    else if (address <= 0x3FFF) {
        romBankLow5_ = value & 0x1F;
    }
    else if (address <= 0x5FFF) {
        bankHigh2_ = value & 0x03;
    }
    else {
        bankingMode_ = value & 1;
    }
}

auto MBC1::ReadRAM(uint16_t address) -> uint8_t {
    if (!ramEnabled_ || ram_.empty()) return 0xFF;

    size_t bank = bankingMode_ ? bankHigh2_ : 0;
    size_t offset = bank * 0x2000 + address - 0xA000;

    return offset < ram_.size() ? ram_[offset] : 0xFF;
}

auto MBC1::WriteRAM(uint16_t address, uint8_t value) -> void {
    if (!ramEnabled_ || ram_.empty()) return;

    size_t bank = bankingMode_ ? bankHigh2_ : 0;
    size_t offset = bank * 0x2000 + address - 0xA000;

    if (offset < ram_.size()) {
        ram_[offset] = value;
    }
}

}
