#include <PixelLink/GameBoy/APU.hpp>
#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/GameBoy/Timer.hpp>
#include <PixelLink/GameBoy/Serial.hpp>

#include <algorithm>

namespace PixelLink::GameBoy {

namespace {
auto IsImplementedAPURegister(const std::uint16_t address) noexcept -> bool {
    return 0xFF10 <= address && address <= 0xFF3F;
}
} // namespace

Bus::Bus() {
    InitializePostBootState();
}

Bus::Bus(Cartridge& cartridge)
    : cartridge_(&cartridge) {
    InitializePostBootState();
}

auto Bus::InsertCartridge(
    Cartridge& cartridge
) noexcept -> void {
    cartridge_ = &cartridge;
}

auto Bus::RemoveCartridge() noexcept -> void {
    cartridge_ = nullptr;
}

auto Bus::AttachPPU(PPU& ppu) noexcept -> void {
    ppu_ = &ppu;
}

auto Bus::DetachPPU(const PPU& ppu) noexcept -> void {
    if (ppu_ == &ppu) {
        ppu_ = nullptr;
    }
}

auto Bus::AttachTimer(Timer& timer) noexcept -> void {
    timer_ = &timer;
}

auto Bus::DetachTimer(const Timer& timer) noexcept -> void {
    if (timer_ == &timer) {
        timer_ = nullptr;
    }
}

auto Bus::AttachAPU(APU& apu) noexcept -> void {
    apu_ = &apu;
}

auto Bus::AttachSerial(Serial& serial) noexcept -> void {
    serial_ = &serial;
}

auto Bus::DetachSerial(const Serial& serial) noexcept -> void {
    if (serial_ == &serial) {
        serial_ = nullptr;
    }
}

auto Bus::DetachAPU(const APU& apu) noexcept -> void {
    if (apu_ == &apu) {
        apu_ = nullptr;
    }
}

auto Bus::AttachJoypad(Joypad& joypad) noexcept -> void {
    joypad_ = &joypad;
}

auto Bus::DetachJoypad(const Joypad& joypad) noexcept -> void {
    if (joypad_ == &joypad) {
        joypad_ = nullptr;
    }
}

auto Bus::Read(
    const std::uint16_t address,
    const BusAccess access
) const -> std::uint8_t {
    // FF46 always returns the last written value, even during OAM DMA.
    if (address == DMA_REGISTER) {
        return io_[DMA_REGISTER - 0xFF00];
    }
    if (access == BusAccess::CPU) {
        if (IsCPUAccessBlockedByDMA(address)) {
            return 0xFF;
        }

        if (IsCPUAccessBlockedByPPU(address)) {
            return 0xFF;
        }
    }

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
        return oam_[address - OAM_BASE];
    }

    // Unusable memory
    if (address <= 0xFEFF) {
        return 0xFF;
    }

    // Joypad register
    if (address == JOYP_REGISTER) {
        if (joypad_ != nullptr) {
            return joypad_->Read();
        }

        return 0xFF;
    }

    if (serial_ != nullptr && (address == 0xFF01 || address == 0xFF02)) {
        return serial_->Read(address);
    }

    // Timer registers
    if (0xFF04 <= address && address <= 0xFF07) {
        if (timer_ != nullptr) {
            return timer_->Read(address);
        }

        return 0xFF;
    }

    if (apu_ != nullptr && IsImplementedAPURegister(address)) {
        return apu_->Read(address);
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
    const std::uint16_t address,
    const std::uint8_t value,
    const BusAccess access
) -> void {
    // Writing FF46 from the CPU starts or restarts OAM DMA.
    // This remains possible while a DMA transfer is already active.
    if (address == DMA_REGISTER &&
        access == BusAccess::CPU) {
        io_[DMA_REGISTER - 0xFF00] = value;
        StartOAMDMA(value);
        return;
    }

    if (access == BusAccess::CPU) {
        if (IsCPUAccessBlockedByDMA(address)) {
            return;
        }

        if (IsCPUAccessBlockedByPPU(address, true)) {
            return;
        }
    }

    // Cartridge ROM / MBC control
    if (address <= 0x7FFF) {
        if (cartridge_ != nullptr) {
            cartridge_->Write(address, value);
        } else {
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
        oam_[address - OAM_BASE] = value;
        return;
    }

    // Unusable memory
    if (address <= 0xFEFF) {
        return;
    }

    // Joypad register
    if (address == JOYP_REGISTER) {
        if (joypad_ != nullptr &&
            joypad_->Write(value)) {
            RequestInterrupt(JOYPAD_INTERRUPT);
        }

        return;
    }

    if (serial_ != nullptr && (address == 0xFF01 || address == 0xFF02)) {
        serial_->Write(address, value);
        return;
    }

    // Timer registers
    if (0xFF04 <= address && address <= 0xFF07) {
        if (timer_ != nullptr) {
            timer_->Write(address, value);
        }

        return;
    }

    if (apu_ != nullptr && IsImplementedAPURegister(address)) {
        apu_->Write(address, value);
        return;
    }

    // I/O registers
    if (address <= 0xFF7F) {
        if (access == BusAccess::CPU && ppu_ != nullptr) {
            if (address == 0xFF44) {
                return; // LY is read-only.
            }
            if (address == 0xFF40 || address == 0xFF41 || address == 0xFF45) {
                const auto oldValue = io_[address - 0xFF00];
                io_[address - 0xFF00] = address == 0xFF41
                    ? static_cast<std::uint8_t>((value & 0x78) | (oldValue & 7) | 0x80)
                    : value;
                ppu_->OnRegisterWrite(address, oldValue);
                return;
            }
        }
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

auto Bus::Tick(const std::uint32_t tCycles) -> void {
    TickOAMDMA(tCycles);
}

auto Bus::RequestInterrupt(
    const std::uint8_t mask
) -> void {
    Write(
        IF_REGISTER,
        static_cast<std::uint8_t>(
            Read(IF_REGISTER, BusAccess::Internal) | mask
        ),
        BusAccess::Internal
    );
}

auto Bus::IsOAMDMAActive() const noexcept -> bool {
    return oamDMAActive_;
}

auto Bus::GetOAMDMABytesTransferred() const noexcept
    -> std::size_t {
    return oamDMAByteIndex_;
}

auto Bus::IsHRAMAddress(
    const std::uint16_t address
) noexcept -> bool {
    return 0xFF80 <= address && address <= 0xFFFE;
}

auto Bus::IsCPUAccessBlockedByDMA(
    const std::uint16_t address
) const noexcept -> bool {
    if (!oamDMATransferRunning_) {
        return false;
    }

    // A VRAM-sourced transfer occupies the video bus and OAM, but the
    // CPU can still fetch instructions from WRAM. This matters when an
    // instruction straddles FDFF/FE00 during the final DMA cycle.
    if (0x8000 <= oamDMASourceBase_ && oamDMASourceBase_ <= 0x9FFF) {
        return (0x8000 <= address && address <= 0x9FFF) ||
               (0xFE00 <= address && address <= 0xFE9F);
    }

    // An external-bus transfer blocks normal CPU accesses except HRAM.
    // FF46 writes are handled before this check so DMA can be restarted.
    return !IsHRAMAddress(address);
}

auto Bus::IsCPUAccessBlockedByPPU(
    const std::uint16_t address, const bool write
) const noexcept -> bool {
    if (ppu_ == nullptr) {
        return false;
    }

    if (0x8000 <= address && address <= 0x9FFF) {
        return !ppu_->CanCPUAccessVRAM(write);
    }

    if (0xFE00 <= address && address <= 0xFE9F) {
        return !ppu_->CanCPUAccessOAM(write);
    }

    return false;
}

auto Bus::InitializePostBootState() noexcept -> void {
    // CPU::Reset() starts execution at the state immediately after the
    // DMG boot ROM. The memory-mapped hardware registers therefore need
    // to start from the corresponding post-boot state as well.
    //
    // In particular, LCDC must start enabled. If FF40 remains zero,
    // PPU::StepOneDot() keeps LY at zero forever, and games that wait for
    // VBlank before doing their own initialization will never progress.
    io_.fill(0);

    // LCD / PPU registers.
    io_[0x40] = 0x91; // FF40 LCDC: LCD on, BG on, unsigned tile data.
    io_[0x42] = 0x00; // FF42 SCY
    io_[0x43] = 0x00; // FF43 SCX
    io_[0x44] = 0x00; // FF44 LY (PPU will maintain this value)
    io_[0x45] = 0x00; // FF45 LYC
    io_[0x47] = 0xFC; // FF47 BGP
    io_[0x48] = 0xFF; // FF48 OBP0
    io_[0x49] = 0xFF; // FF49 OBP1
    io_[0x4A] = 0x00; // FF4A WY
    io_[0x4B] = 0x00; // FF4B WX

    ie_ = 0x00;
}

auto Bus::StartOAMDMA(const std::uint8_t sourceHigh) -> void {
    // The write M-cycle and the following M-cycle precede the new transfer.
    // A restart keeps the old source and CPU restrictions until that edge.
    oamDMAActive_ = true;
    const auto mappedHigh = sourceHigh >= 0xE0 ? sourceHigh - 0x20 : sourceHigh;
    oamDMAPendingSourceBase_ = static_cast<std::uint16_t>(mappedHigh << 8u);
    oamDMAStartupCyclesRemaining_ = 8;
}

auto Bus::TickOAMDMA(const std::uint32_t tCycles) -> void {
    for (std::uint32_t cycle = 0; cycle < tCycles && oamDMAActive_; ++cycle) {
        if (oamDMATransferRunning_ && ++oamDMATCycleAccumulator_ == 4) {
            oamDMATCycleAccumulator_ = 0;
            const auto source = static_cast<std::uint16_t>(
                oamDMASourceBase_ + oamDMAByteIndex_);
            const auto destination = static_cast<std::uint16_t>(
                OAM_BASE + oamDMAByteIndex_);
            Write(destination, Read(source, BusAccess::DMA), BusAccess::DMA);
            if (++oamDMAByteIndex_ == OAM_DMA_BYTES) {
                oamDMATransferRunning_ = false;
                oamDMAActive_ = oamDMAStartupCyclesRemaining_ != 0;
            }
        }
        if (oamDMAStartupCyclesRemaining_ != 0 &&
            --oamDMAStartupCyclesRemaining_ == 0) {
            oamDMASourceBase_ = oamDMAPendingSourceBase_;
            oamDMAByteIndex_ = 0;
            oamDMATCycleAccumulator_ = 0;
            oamDMATransferRunning_ = true;
            oamDMAActive_ = true;
        }
    }
}

} // namespace PixelLink::GameBoy
