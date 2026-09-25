#include "PixelLink/GameBoy/Mapper/MBC5.hpp"

namespace PixelLink::GameBoy {

MBC5::MBC5(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram)
    : rom_(rom), ram_(ram) {
}

auto MBC5::ReadROM(uint16_t address) const -> uint8_t {
    size_t bank = address < 0x4000 ? 0 : romBank_;

    if (address >= 0x4000)
        address -= 0x4000;

    size_t offset = bank * 0x4000 + address;

    return offset < rom_.size() ? rom_[offset] : 0xFF;
}

auto MBC5::WriteROM(uint16_t address, uint8_t value) -> void {
    if (address <= 0x1FFF) {
        ramEnabled_ = (value & 0x0F) == 0x0A;
    }
    else if (address <= 0x2FFF) {
        romBank_ = (romBank_ & 0x100) | value;
    }
    else if (address <= 0x3FFF) {
        romBank_ = (romBank_ & 0xFF) | ((value & 1) << 8);
    }
    else if (address <= 0x5FFF) {
        ramBank_ = value & 0x0F;
    }
}

auto MBC5::ReadRAM(uint16_t address) -> uint8_t {
    if (!ramEnabled_ || ram_.empty()) return 0xFF;

    size_t offset =
        ramBank_ * 0x2000 +
        address - 0xA000;

    return offset < ram_.size() ? ram_[offset] : 0xFF;
}

auto MBC5::WriteRAM(uint16_t address, uint8_t value) -> void {
    if (!ramEnabled_ || ram_.empty()) return;

    size_t offset =
        ramBank_ * 0x2000 +
        address - 0xA000;

    if (offset < ram_.size()) {
        ram_[offset] = value;
    }
}

}
