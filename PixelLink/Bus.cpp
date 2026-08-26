#include "Bus.hpp"

Bus::Bus(Cartridge& cartridge)
    : cartridge_(&cartridge) {
}

auto Bus::insertCartridge(
    Cartridge& cartridge
) noexcept -> void {
    cartridge_ = &cartridge;
}

auto Bus::removeCartridge() noexcept -> void {
    cartridge_ = nullptr;
}

auto Bus::read(uint16_t address) const -> uint8_t {
    // Cartridge ROM
    if (address <= 0x7FFF) {
        if (cartridge_ != nullptr) {
            return cartridge_->read(address);
        }

        return testRom_[address];
    }

    // VRAM
    if (address <= 0x9FFF) {
        return vram_[address - 0x8000];
    }

    // Cartridge RAM / external hardware
    if (address <= 0xBFFF) {
        if (cartridge_ != nullptr) {
            return cartridge_->read(address);
        }

        return 0xFF;
    }

    // WRAM
    if (address <= 0xDFFF) {
        return wram_[address - 0xC000];
    }

    // Echo RAM
    if (address <= 0xFDFF) {
        return wram_[address - 0xE000];
    }

    // OAM
    if (address <= 0xFE9F) {
        return oam_[address - 0xFE00];
    }

    // Unusable memory
    if (address <= 0xFEFF) {
        return 0xFF;
    }

    // I/O registers
    if (address <= 0xFF7F) {
        return io_[address - 0xFF00];
    }

    // HRAM
    if (address <= 0xFFFE) {
        return hram_[address - 0xFF80];
    }

    // Interrupt Enable
    return ie_;
}

auto Bus::write(
    uint16_t address,
    uint8_t value
) -> void {
    // Cartridge ROM / MBC control
    if (address <= 0x7FFF) {
        if (cartridge_ != nullptr) {
            cartridge_->write(address, value);
        }
        else {
            testRom_[address] = value;
        }

        return;
    }

    // VRAM
    if (address <= 0x9FFF) {
        vram_[address - 0x8000] = value;
        return;
    }

    // Cartridge RAM / external hardware
    if (address <= 0xBFFF) {
        if (cartridge_ != nullptr) {
            cartridge_->write(address, value);
        }

        return;
    }

    // WRAM
    if (address <= 0xDFFF) {
        wram_[address - 0xC000] = value;
        return;
    }

    // Echo RAM
    if (address <= 0xFDFF) {
        wram_[address - 0xE000] = value;
        return;
    }

    // OAM
    if (address <= 0xFE9F) {
        oam_[address - 0xFE00] = value;
        return;
    }

    // Unusable memory
    if (address <= 0xFEFF) {
        return;
    }

    // I/O registers
    if (address <= 0xFF7F) {
        io_[address - 0xFF00] = value;
        return;
    }

    // HRAM
    if (address <= 0xFFFE) {
        hram_[address - 0xFF80] = value;
        return;
    }

    // Interrupt Enable
    ie_ = value;
}