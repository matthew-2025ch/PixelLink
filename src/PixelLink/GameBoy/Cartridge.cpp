#include <fstream>
#include <format>
#include <iterator>
#include <stdexcept>

#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Mapper/ROMOnly.hpp>
#include <PixelLink/GameBoy/Mapper/MBC1.hpp>
#include <PixelLink/GameBoy/Mapper/MBC3.hpp>
#include <PixelLink/GameBoy/Mapper/MBC5.hpp>

namespace PixelLink::GameBoy {

auto Cartridge::Load(
    const std::filesystem::path& path
) -> void
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Failed to open ROM file"
        );
    }

    rom.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    if (rom.empty()) {
        throw std::runtime_error(
            "ROM is empty"
        );
    }

    ParseHeader();
    ConfigureMapper();
}

auto Cartridge::Read(
    uint16_t address
) const -> uint8_t
{
    if (!mapper_) {
        return 0xFF;
    }

    if (address <= 0x7FFF) {
        return mapper_->ReadROM(address);
    }

    if (address >= 0xA000 &&
        address <= 0xBFFF) {
        return mapper_->ReadRAM(address);
    }

    return 0xFF;
}

auto Cartridge::Write(
    uint16_t address,
    uint8_t value
) -> void
{
    if (!mapper_) {
        return;
    }

    if (address <= 0x7FFF) {
        mapper_->WriteROM(address, value);
        return;
    }

    if (address >= 0xA000 &&
        address <= 0xBFFF) {
        mapper_->WriteRAM(address, value);
    }
}

auto Cartridge::ParseHeader() -> void
{
    if (rom.size() < 0x150) {
        throw std::runtime_error(
            "Invalid ROM size"
        );
    }

    cartridgeHeader.type = rom[0x147];
    cartridgeHeader.romSizeCode = rom[0x148];
    cartridgeHeader.ramSizeCode = rom[0x149];

    cartridgeHeader.title.clear();

    for (uint16_t i = 0x134;
         i <= 0x143;
         i++) {

        if (rom[i] == 0) {
            break;
        }

        cartridgeHeader.title.push_back(
            static_cast<char>(rom[i])
        );
    }

    cartridgeHeader.headerChecksum = rom[0x14D];

    cartridgeHeader.declaredRomSize =
        DecodeROMSize(
            cartridgeHeader.romSizeCode
        );

    cartridgeHeader.declaredRamSize =
        DecodeRAMSize(
            cartridgeHeader.ramSizeCode
        );

    cartridgeHeader.headerChecksumValid =
        CalculateHeaderChecksum()
        == cartridgeHeader.headerChecksum;
}

auto Cartridge::ConfigureMapper() -> void
{
    ram.resize(
        cartridgeHeader.declaredRamSize,
        0x00
    );

    switch (cartridgeHeader.type)
    {
    case 0x00:
        mapper_ =
            std::make_unique<ROMOnly>(rom);
        break;

    case 0x01:
    case 0x02:
    case 0x03:
        mapper_ =
            std::make_unique<MBC1>(
                rom,
                ram
            );
        break;

    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
        mapper_ =
            std::make_unique<MBC3>(
                rom,
                ram
            );
        break;

    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
        mapper_ =
            std::make_unique<MBC5>(
                rom,
                ram
            );
        break;

    default:
        throw std::runtime_error(
            std::format(
                "Unsupported cartridge type: 0x{:02X}",
                cartridgeHeader.type
            )
        );
    }
}

auto Cartridge::Loaded() const noexcept -> bool
{
    return !rom.empty();
}

auto Cartridge::Size() const noexcept -> std::size_t
{
    return rom.size();
}

auto Cartridge::Header() const noexcept
    -> const CartridgeHeader&
{
    return cartridgeHeader;
}

auto Cartridge::CalculateHeaderChecksum()
    const -> uint8_t
{
    uint8_t checksum = 0;

    for (uint16_t address = 0x134;
         address <= 0x14C;
         address++) {

        checksum =
            checksum -
            rom[address] -
            1;
    }

    return checksum;
}

auto Cartridge::DecodeROMSize(
    uint8_t code
) -> std::size_t
{
    switch (code)
    {
    case 0x00: return 32 * 1024;
    case 0x01: return 64 * 1024;
    case 0x02: return 128 * 1024;
    case 0x03: return 256 * 1024;
    case 0x04: return 512 * 1024;
    case 0x05: return 1024 * 1024;
    case 0x06: return 2 * 1024 * 1024;
    case 0x07: return 4 * 1024 * 1024;
    case 0x08: return 8 * 1024 * 1024;
    default:
        return 0;
    }
}

auto Cartridge::DecodeRAMSize(
    uint8_t code
) -> std::size_t
{
    switch (code)
    {
    case 0x00: return 0;
    case 0x01: return 2 * 1024;
    case 0x02: return 8 * 1024;
    case 0x03: return 32 * 1024;
    case 0x04: return 128 * 1024;
    case 0x05: return 64 * 1024;
    default:
        return 0;
    }
}

auto CartridgeTypeName(
    uint8_t type
) -> std::string_view
{
    switch(type)
    {
    case 0x00: return "ROM ONLY";
    case 0x01: return "MBC1";
    case 0x03: return "MBC1+RAM+BATTERY";
    case 0x13: return "MBC3+RAM+BATTERY";
    case 0x1B: return "MBC5+RAM+BATTERY";
    default: return "Unknown";
    }
}

}
