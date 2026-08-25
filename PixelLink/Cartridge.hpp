#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

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

    auto load(const std::filesystem::path& path) -> void;

    auto read(uint16_t address) const -> uint8_t;
    auto write(uint16_t address, uint8_t value) -> void;

    [[nodiscard]] auto loaded() const noexcept -> bool;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto header() const noexcept -> const CartridgeHeader&;
    auto printInfo(std::ostream& os) const -> void;

private:
    std::vector<uint8_t> rom;
    CartridgeHeader cartridgeHeader;

    auto parseHeader() -> void;

    [[nodiscard]] auto calculateHeaderChecksum() const -> uint8_t;
    [[nodiscard]] static auto decodeRomSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto decodeRamSize(uint8_t code) -> std::size_t;
    [[nodiscard]] static auto cartridgeTypeName(uint8_t type) -> std::string_view;
};