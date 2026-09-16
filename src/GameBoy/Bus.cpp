#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/GameBoy/PPU.hpp>
#include <PixelLink/GameBoy/Timer.hpp>

namespace PixelLink::GameBoy {

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

    // Timer registers
    if (0xFF04 <= address && address <= 0xFF07) {
        if (timer_ != nullptr) {
            return timer_->Read(address);
        }

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

        if (IsCPUAccessBlockedByPPU(address)) {
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

    // Timer registers
    if (0xFF04 <= address && address <= 0xFF07) {
        if (timer_ != nullptr) {
            timer_->Write(address, value);
        }

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
    if (!oamDMAActive_) {
        return false;
    }

    // DMG behavior: while OAM DMA is active, CPU accesses are
    // restricted to HRAM. FF46 writes are handled before this check
    // so that an active transfer can be restarted.
    return !IsHRAMAddress(address);
}

auto Bus::IsCPUAccessBlockedByPPU(
    const std::uint16_t address
) const noexcept -> bool {
    if (ppu_ == nullptr) {
        return false;
    }

    if (0x8000 <= address && address <= 0x9FFF) {
        return !ppu_->CanCPUAccessVRAM();
    }

    if (0xFE00 <= address && address <= 0xFE9F) {
        return !ppu_->CanCPUAccessOAM();
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

auto Bus::StartOAMDMA(
    const std::uint8_t sourceHigh
) -> void {
    oamDMAActive_ = true;
    oamDMASourceBase_ =
        static_cast<std::uint16_t>(sourceHigh) << 8u;
    oamDMAByteIndex_ = 0;
    oamDMATCycleAccumulator_ = 0;
}

auto Bus::TickOAMDMA(
    const std::uint32_t tCycles
) -> void {
    if (!oamDMAActive_) {
        return;
    }

    oamDMATCycleAccumulator_ += tCycles;

    while (
        oamDMAActive_ &&
        oamDMATCycleAccumulator_ >=
            OAM_DMA_T_CYCLES_PER_BYTE
    ) {
        oamDMATCycleAccumulator_ -=
            OAM_DMA_T_CYCLES_PER_BYTE;

        const std::uint16_t sourceAddress =
            static_cast<std::uint16_t>(
                oamDMASourceBase_ +
                static_cast<std::uint16_t>(
                    oamDMAByteIndex_
                )
            );

        const std::uint16_t destinationAddress =
            static_cast<std::uint16_t>(
                OAM_BASE +
                static_cast<std::uint16_t>(
                    oamDMAByteIndex_
                )
            );

        const std::uint8_t value =
            Read(sourceAddress, BusAccess::DMA);

        Write(
            destinationAddress,
            value,
            BusAccess::DMA
        );

        ++oamDMAByteIndex_;

        if (oamDMAByteIndex_ >= OAM_DMA_BYTES) {
            oamDMAActive_ = false;
            oamDMATCycleAccumulator_ = 0;
        }
    }
}

} // namespace PixelLink::GameBoy
