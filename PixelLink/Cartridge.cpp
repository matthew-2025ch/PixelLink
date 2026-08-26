#include "Cartridge.hpp"

#include <format>
#include <fstream>
#include <ostream>
#include <stdexcept>

auto Cartridge::load(const std::filesystem::path& path) -> void {
    std::ifstream file(
        path,
        std::ios::binary | std::ios::ate
    );

    if (!file) {
        throw std::runtime_error(
            std::format(
                "Failed to open ROM file: {}",
                path.string()
            )
        );
    }

    const std::streamsize fileSize = file.tellg();

    if (fileSize < 0x150) {
        throw std::runtime_error(
            std::format(
                "ROM file is too small: {} bytes",
                fileSize
            )
        );
    }

    rom.resize(
        static_cast<std::size_t>(fileSize)
    );

    file.seekg(0, std::ios::beg);

    if (!file.read(
        reinterpret_cast<char*>(rom.data()),
        fileSize
    )) {
        throw std::runtime_error(
            std::format(
                "Failed to read ROM file: {}",
                path.string()
            )
        );
    }

    parseHeader();

    if (cartridgeHeader.type != 0x00) {
        throw std::runtime_error(
            std::format(
                "Unsupported cartridge type: {} (0x{:02X})",
                cartridgeTypeName(cartridgeHeader.type),
                static_cast<unsigned>(cartridgeHeader.type)
            )
        );
    }
}

auto Cartridge::read(
    uint16_t address
) const -> uint8_t {
    // ROM
    if (address <= 0x7FFF) {
        if (!loaded()) {
            return 0xFF;
        }

        if (address >= rom.size()) {
            return 0xFF;
        }

        return rom[address];
    }

    // External cartridge RAM
    if (
        address >= 0xA000 &&
        address <= 0xBFFF
        ) {
        // ROM-only cartridges do not provide external RAM yet.
        return 0xFF;
    }

    return 0xFF;
}

auto Cartridge::write(
    uint16_t address,
    uint8_t value
) -> void {
    // ROM / mapper control
    if (address <= 0x7FFF) {
        // ROM-only cartridges ignore writes.
        return;
    }

    // External cartridge RAM
    if (
        address >= 0xA000 &&
        address <= 0xBFFF
        ) {
        // ROM-only cartridges do not provide external RAM yet.
        return;
    }

    (void)value;
}

auto Cartridge::loaded() const noexcept -> bool {
    return !rom.empty();
}

auto Cartridge::size() const noexcept -> std::size_t {
    return rom.size();
}

auto Cartridge::header() const noexcept
-> const CartridgeHeader& {
    return cartridgeHeader;
}

auto Cartridge::parseHeader() -> void {
    cartridgeHeader = {};

    for (
        std::size_t address = 0x0134;
        address <= 0x0143;
        ++address
        ) {
        const uint8_t value = rom[address];

        if (address == 0x0143 &&
            (value == 0x80 || value == 0xC0)) {
            break;
        }

        if (value == 0x00) {
            break;
        }

        if (value < 0x20 || value > 0x7E) {
            break;
        }

        cartridgeHeader.title.push_back(
            static_cast<char>(value)
        );
    }

    cartridgeHeader.type =
        rom[0x0147];

    cartridgeHeader.romSizeCode =
        rom[0x0148];

    cartridgeHeader.ramSizeCode =
        rom[0x0149];

    cartridgeHeader.headerChecksum =
        rom[0x014D];

    cartridgeHeader.declaredRomSize =
        decodeRomSize(
            cartridgeHeader.romSizeCode
        );

    cartridgeHeader.declaredRamSize =
        decodeRamSize(
            cartridgeHeader.ramSizeCode
        );

    cartridgeHeader.headerChecksumValid =
        calculateHeaderChecksum()
        ==
        cartridgeHeader.headerChecksum;
}

auto Cartridge::calculateHeaderChecksum() const
-> uint8_t {
    uint8_t checksum = 0;

    for (
        uint16_t address = 0x0134;
        address <= 0x014C;
        ++address
        ) {
        checksum = static_cast<uint8_t>(
            checksum
            - rom[address]
            - 1
            );
    }

    return checksum;
}

auto Cartridge::decodeRomSize(uint8_t code)
-> std::size_t {
    if (code <= 0x08) {
        return
            static_cast<std::size_t>(32 * 1024)
            << code;
    }

    switch (code) {
    case 0x52:
        return 72 * 16 * 1024;

    case 0x53:
        return 80 * 16 * 1024;

    case 0x54:
        return 96 * 16 * 1024;

    default:
        return 0;
    }
}

auto Cartridge::decodeRamSize(uint8_t code)
-> std::size_t {
    switch (code) {
    case 0x00:
        return 0;

    case 0x02:
        return 8 * 1024;

    case 0x03:
        return 32 * 1024;

    case 0x04:
        return 128 * 1024;

    case 0x05:
        return 64 * 1024;

    default:
        return 0;
    }
}

auto Cartridge::cartridgeTypeName(uint8_t type)
-> std::string_view {
    switch (type) {
    case 0x00:
        return "ROM ONLY";

    case 0x01:
        return "MBC1";

    case 0x02:
        return "MBC1+RAM";

    case 0x03:
        return "MBC1+RAM+BATTERY";

    case 0x05:
        return "MBC2";

    case 0x06:
        return "MBC2+BATTERY";

    case 0x08:
        return "ROM+RAM";

    case 0x09:
        return "ROM+RAM+BATTERY";

    case 0x0F:
        return "MBC3+TIMER+BATTERY";

    case 0x10:
        return "MBC3+TIMER+RAM+BATTERY";

    case 0x11:
        return "MBC3";

    case 0x12:
        return "MBC3+RAM";

    case 0x13:
        return "MBC3+RAM+BATTERY";

    case 0x19:
        return "MBC5";

    case 0x1A:
        return "MBC5+RAM";

    case 0x1B:
        return "MBC5+RAM+BATTERY";

    case 0x1C:
        return "MBC5+RUMBLE";

    case 0x1D:
        return "MBC5+RUMBLE+RAM";

    case 0x1E:
        return "MBC5+RUMBLE+RAM+BATTERY";

    default:
        return "UNKNOWN";
    }
}

auto Cartridge::printInfo(std::ostream& os) const -> void {
    if (!loaded()) {
        os << "No cartridge loaded.\n";
        return;
    }

    os << std::format(
        "Title: {}\n"
        "Cartridge type: {} (0x{:02X})\n"
        "ROM size: {} KiB\n"
        "ROM file size: {} KiB\n"
        "RAM size: {} KiB\n"
        "Header checksum: 0x{:02X}\n"
        "Header checksum valid: {}\n",
        cartridgeHeader.title,
        cartridgeTypeName(cartridgeHeader.type),
        static_cast<unsigned>(
            cartridgeHeader.type
            ),
        cartridgeHeader.declaredRomSize / 1024,
        rom.size() / 1024,
        cartridgeHeader.declaredRamSize / 1024,
        static_cast<unsigned>(
            cartridgeHeader.headerChecksum
            ),
        cartridgeHeader.headerChecksumValid
        ? "yes"
        : "no"
    );
}