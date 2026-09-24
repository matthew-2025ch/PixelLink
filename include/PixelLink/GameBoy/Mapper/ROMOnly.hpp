#pragma once
#include "PixelLink/GameBoy/Mapper/Mapper.hpp"
#include <vector>

namespace PixelLink::GameBoy {

class ROMOnly final : public Mapper {
public:
    explicit ROMOnly(std::vector<uint8_t>& rom);

    auto ReadROM(uint16_t address) const -> uint8_t override;
    auto WriteROM(uint16_t address, uint8_t value) -> void override;

private:
    std::vector<uint8_t>& rom_;
};

}
