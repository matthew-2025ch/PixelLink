#include "PixelLink/GameBoy/Mapper/ROMOnly.hpp"

namespace PixelLink::GameBoy {

ROMOnly::ROMOnly(std::vector<uint8_t>& rom)
    : rom_(rom) {
}

auto ROMOnly::ReadROM(uint16_t address) const -> uint8_t {
    if (address >= rom_.size()) {
        return 0xFF;
    }

    return rom_[address];
}

auto ROMOnly::WriteROM(uint16_t, uint8_t) -> void {
}

}
