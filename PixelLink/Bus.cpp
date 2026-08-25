#include "Bus.hpp"

Bus::Bus(Cartridge& cartridge)
    : cartridge(&cartridge) {
}

auto Bus::insertCartridge(
    Cartridge& cartridge
) noexcept -> void {
    this->cartridge = &cartridge;
}

auto Bus::removeCartridge() noexcept -> void {
    cartridge = nullptr;
}

auto Bus::read(uint16_t address) const
-> uint8_t {
    if (
        cartridge != nullptr &&
        address <= 0x7FFF
        ) {
        return cartridge->read(address);
    }

    return memory[address];
}

auto Bus::write(
    uint16_t address,
    uint8_t value
) -> void {
    if (
        cartridge != nullptr &&
        address <= 0x7FFF
        ) {
        cartridge->write(
            address,
            value
        );

        return;
    }

    memory[address] = value;
}