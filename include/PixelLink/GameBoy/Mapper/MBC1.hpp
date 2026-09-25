#pragma once
#include "PixelLink/GameBoy/Mapper/Mapper.hpp"
#include <vector>

namespace PixelLink::GameBoy {

class MBC1 final : public Mapper {
public:
    MBC1(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram);

    auto ReadROM(uint16_t address) const -> uint8_t override;
    auto WriteROM(uint16_t address, uint8_t value) -> void override;

    auto ReadRAM(uint16_t address) -> uint8_t override;
    auto WriteRAM(uint16_t address, uint8_t value) -> void override;

private:
    std::vector<uint8_t>& rom_;
    std::vector<uint8_t>& ram_;

    bool ramEnabled_ = false;
    uint8_t romBankLow5_ = 1;
    uint8_t bankHigh2_ = 0;
    uint8_t bankingMode_ = 0;
};

}
