#include "Bus.hpp"

namespace PixelLink::GameBoy {

Bus::Bus(Cartridge& cartridge)
    : cartridge_(&cartridge) {
}

auto Bus::InsertCartridge(
    Cartridge& cartridge
) noexcept -> void {
    cartridge_ = &cartridge;
}

auto Bus::RemoveCartridge() noexcept -> void {
    cartridge_ = nullptr;
}

auto Bus::Read(uint16_t address) const -> uint8_t {
    // Cartridge ROM
    if (address <= 0x7FFF) {
        if (cartridge_ != nullptr) {
            return cartridge_->Read(address);
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
            return cartridge_->Read(address);
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

    // Timer registers
    if (0xFF04 <= address && address <= 0xFF07) {
        return timer_.Read(address);
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

auto Bus::Write(
    uint16_t address,
    uint8_t value
) -> void {
    // Cartridge ROM / MBC control
    if (address <= 0x7FFF) {
        if (cartridge_ != nullptr) {
            cartridge_->Write(address, value);
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
            cartridge_->Write(address, value);
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

    // Timer registers mapping
    if (0xFF04 <= address && address <= 0xFF07) {
        timer_.Write(address, value);
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

auto Bus::Tick(uint32_t tCycles)->void {
	timer_.Tick(tCycles);
    if (timer_.ConsumeInterruptRequest()) {
        constexpr uint16_t IF = 0xFF0F;
        constexpr uint8_t TIMER_INTERRUPT = 1u << 2;
        Write(IF, static_cast<uint8_t>(Read(IF) | TIMER_INTERRUPT));
    }
}

} // namespace PixelLink::GameBoy