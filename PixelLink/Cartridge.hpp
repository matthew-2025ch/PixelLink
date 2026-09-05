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
    std::vector<uint8_t> rom;
    CartridgeHeader cartridgeHeader;

    auto ParseHeader() -> void;

    [[nodiscard]] auto CalculateHeaderChecksum() const -> uint8_t;
    [[nodiscard]] static auto DecodeROMSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto DecodeRAMSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto CartridgeTypeName(uint8_t type) -> std::string_view;
};

} // namespace PixelLink::GameBoy