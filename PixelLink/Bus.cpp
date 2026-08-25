#include "Bus.hpp"

auto Bus::read(uint16_t address) const -> uint8_t {
    return memory[address];
}

auto Bus::write(uint16_t address, uint8_t value) -> void {
    memory[address] = value;
}
