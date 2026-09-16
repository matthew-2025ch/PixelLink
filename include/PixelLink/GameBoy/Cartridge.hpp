#pragma once

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
    };

    static constexpr std::size_t ROM_BANK_SIZE = 0x4000;
    static constexpr std::size_t RAM_BANK_SIZE = 0x2000;

    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    CartridgeHeader cartridgeHeader;

    Mapper mapper_ = Mapper::ROMOnly;

    // MBC1 state.
    bool ramEnabled_ = false;
    uint8_t romBankLow5_ = 1;
    uint8_t bankHigh2_ = 0;
    uint8_t bankingMode_ = 0;

    auto ParseHeader() -> void;
    auto ConfigureMapper() -> void;
    auto ResetMapperState() noexcept -> void;

    [[nodiscard]] auto ReadROMOnly(uint16_t address) const -> uint8_t;
    [[nodiscard]] auto ReadMBC1(uint16_t address) const -> uint8_t;
    auto WriteMBC1(uint16_t address, uint8_t value) -> void;

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
