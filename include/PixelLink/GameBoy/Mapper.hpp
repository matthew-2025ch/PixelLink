#pragma once

#include <cstdint>

namespace PixelLink::GameBoy {

class Mapper {
public:
    virtual ~Mapper() = default;

    virtual auto ReadROM(uint16_t address) const -> uint8_t = 0;
    virtual auto WriteROM(uint16_t address, uint8_t value) -> void = 0;

    virtual auto ReadRAM(uint16_t address) const -> uint8_t {
        return 0xFF;
    }

    virtual auto WriteRAM(uint16_t address, uint8_t value) -> void {
    }
};

}
