#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PixelLink::GameBoy {

class Cartridge;
class Joypad;
class PPU;
class Timer;

enum class BusAccess : std::uint8_t {
    CPU,
    PPU,
    DMA,
    Internal,
};

class Bus {
public:
    Bus();

    explicit Bus(Cartridge& cartridge);

    auto InsertCartridge(Cartridge& cartridge) noexcept -> void;
    auto RemoveCartridge() noexcept -> void;

    auto AttachPPU(PPU& ppu) noexcept -> void;
    auto DetachPPU(const PPU& ppu) noexcept -> void;

    auto AttachTimer(Timer& timer) noexcept -> void;
    auto DetachTimer(const Timer& timer) noexcept -> void;

    auto AttachJoypad(Joypad& joypad) noexcept -> void;
    auto DetachJoypad(const Joypad& joypad) noexcept -> void;

    [[nodiscard]] auto Read(
        std::uint16_t address,
        BusAccess access = BusAccess::CPU
    ) const -> std::uint8_t;

    auto Write(
        std::uint16_t address,
        std::uint8_t value,
        BusAccess access = BusAccess::CPU
    ) -> void;

    // Advances devices that are part of the bus itself.
    // Timer/PPU are advanced by GameBoy, because GameBoy owns them.
    auto Tick(std::uint32_t tCycles) -> void;

    // Hardware devices use this to raise a bit in IF (FF0F).
    auto RequestInterrupt(std::uint8_t mask) -> void;

    [[nodiscard]] auto IsOAMDMAActive() const noexcept -> bool;
    [[nodiscard]] auto GetOAMDMABytesTransferred() const noexcept
        -> std::size_t;

private:
    static constexpr std::uint16_t JOYP_REGISTER = 0xFF00;
    static constexpr std::uint16_t IF_REGISTER = 0xFF0F;
    static constexpr std::uint8_t JOYPAD_INTERRUPT = 1u << 4;

    static constexpr std::uint16_t DMA_REGISTER = 0xFF46;
    static constexpr std::uint16_t OAM_BASE = 0xFE00;
    static constexpr std::size_t OAM_DMA_BYTES = 0x00A0;
    static constexpr std::uint32_t OAM_DMA_T_CYCLES_PER_BYTE = 4;

    Cartridge* cartridge_ = nullptr;
    PPU* ppu_ = nullptr;
    Timer* timer_ = nullptr;
    Joypad* joypad_ = nullptr;

    // Used only when no cartridge is inserted.
    // This keeps CPU unit tests simple.
    std::array<std::uint8_t, 0x8000> testRom_{};
    std::array<std::uint8_t, 0x2000> vram_{};
    std::array<std::uint8_t, 0x2000> wram_{};
    std::array<std::uint8_t, 0x00A0> oam_{};
    std::array<std::uint8_t, 0x0080> io_{};
    std::array<std::uint8_t, 0x007F> hram_{};
    std::uint8_t ie_ = 0;

    bool oamDMAActive_ = false;
    std::uint16_t oamDMASourceBase_ = 0;
    std::size_t oamDMAByteIndex_ = 0;
    std::uint32_t oamDMATCycleAccumulator_ = 0;

    [[nodiscard]] static auto IsHRAMAddress(
        std::uint16_t address
    ) noexcept -> bool;

    [[nodiscard]] auto IsCPUAccessBlockedByDMA(
        std::uint16_t address
    ) const noexcept -> bool;

    [[nodiscard]] auto IsCPUAccessBlockedByPPU(
        std::uint16_t address
    ) const noexcept -> bool;

    auto InitializePostBootState() noexcept -> void;

    auto StartOAMDMA(std::uint8_t sourceHigh) -> void;
    auto TickOAMDMA(std::uint32_t tCycles) -> void;
};

} // namespace PixelLink::GameBoy
