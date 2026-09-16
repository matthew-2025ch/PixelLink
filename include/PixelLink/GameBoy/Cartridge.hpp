#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace PixelLink::GameBoy {

struct CartridgeHeader {
    std::string title;

    uint8_t type = 0;
    uint8_t romSizeCode = 0;
    uint8_t ramSizeCode = 0;
    uint8_t headerChecksum = 0;

    std::size_t declaredRomSize = 0;
    std::size_t declaredRamSize = 0;

    bool headerChecksumValid = false;
};

class Cartridge {
public:
    Cartridge() = default;

    auto Load(const std::filesystem::path& path) -> void;

    auto Read(uint16_t address) const -> uint8_t;
    auto Write(uint16_t address, uint8_t value) -> void;

    [[nodiscard]] auto Loaded() const noexcept -> bool;
    [[nodiscard]] auto Size() const noexcept -> std::size_t;
    [[nodiscard]] auto Header() const noexcept -> const CartridgeHeader&;
    auto PrintInfo(std::ostream& os) const -> void;

private:
    enum class Mapper : uint8_t {
        ROMOnly,
        MBC1,
        MBC3,
    };

    struct RTCState {
        uint8_t seconds = 0;
        uint8_t minutes = 0;
        uint8_t hours = 0;
        uint16_t days = 0;
        bool halt = false;
        bool carry = false;
    };

    static constexpr std::size_t ROM_BANK_SIZE = 0x4000;
    static constexpr std::size_t RAM_BANK_SIZE = 0x2000;

    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    CartridgeHeader cartridgeHeader;

    Mapper mapper_ = Mapper::ROMOnly;

    // Shared RAM enable latch used by MBC1 and MBC3.
    bool ramEnabled_ = false;

    // MBC1 state.
    uint8_t romBankLow5_ = 1;
    uint8_t bankHigh2_ = 0;
    uint8_t bankingMode_ = 0;

    // MBC3 state.
    bool hasRTC_ = false;
    uint8_t mbc3RomBank_ = 1;
    uint8_t mbc3RamRtcSelect_ = 0;
    uint8_t mbc3LastLatchWrite_ = 0xFF;

    mutable RTCState rtc_{};
    mutable std::array<uint8_t, 5> latchedRTC_{};
    mutable bool rtcLatchedValid_ = false;
    mutable std::chrono::steady_clock::time_point rtcLastUpdate_ =
        std::chrono::steady_clock::now();

    auto ParseHeader() -> void;
    auto ConfigureMapper() -> void;
    auto ResetMapperState() noexcept -> void;

    [[nodiscard]] auto ReadROMOnly(uint16_t address) const -> uint8_t;

    [[nodiscard]] auto ReadMBC1(uint16_t address) const -> uint8_t;
    [[nodiscard]] auto ReadMBC1RAM(uint16_t address) const -> uint8_t;
    auto WriteMBC1(uint16_t address, uint8_t value) -> void;
    auto WriteMBC1RAM(uint16_t address, uint8_t value) -> void;

    [[nodiscard]] auto ReadMBC3(uint16_t address) const -> uint8_t;
    [[nodiscard]] auto ReadMBC3RAMRTC(uint16_t address) const -> uint8_t;
    auto WriteMBC3(uint16_t address, uint8_t value) -> void;
    auto WriteMBC3RAMRTC(uint16_t address, uint8_t value) -> void;

    auto SyncRTC() const -> void;
    auto LatchRTC() const -> void;
    [[nodiscard]] auto ReadRTCRegister(uint8_t reg) const -> uint8_t;
    auto WriteRTCRegister(uint8_t reg, uint8_t value) -> void;
    [[nodiscard]] auto RTCRegisterValue(uint8_t reg) const -> uint8_t;

    [[nodiscard]] auto ReadROMBank(
        std::size_t bank,
        uint16_t bankAddress
    ) const -> uint8_t;

    [[nodiscard]] auto ROMBankCount() const noexcept -> std::size_t;
    [[nodiscard]] auto RAMBankCount() const noexcept -> std::size_t;

    [[nodiscard]] auto CalculateHeaderChecksum() const -> uint8_t;
    [[nodiscard]] static auto DecodeROMSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto DecodeRAMSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto CartridgeTypeName(uint8_t type) -> std::string_view;
};

} // namespace PixelLink::GameBoy
