#include <format>
#include <fstream>
#include <ostream>
#include <stdexcept>

#include <PixelLink/GameBoy/Cartridge.hpp>

namespace PixelLink::GameBoy {

auto Cartridge::Load(const std::filesystem::path& path) -> void {
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
                "Failed to Read ROM file: {}",
                path.string()
            )
        );
    }

    ParseHeader();
    ConfigureMapper();
    ResetMapperState();
}

auto Cartridge::Read(
    const uint16_t address
) const -> uint8_t {
    if (!Loaded()) {
        return 0xFF;
    }

    if (address <= 0x7FFF) {
        switch (mapper_) {
        case Mapper::ROMOnly:
            return ReadROMOnly(address);

        case Mapper::MBC1:
            return ReadMBC1(address);
        }
    }

    if (0xA000 <= address && address <= 0xBFFF) {
        if (mapper_ != Mapper::MBC1 ||
            !ramEnabled_ ||
            ram.empty()) {
            return 0xFF;
        }

        std::size_t ramBank = 0;

        if (bankingMode_ == 1) {
            ramBank = bankHigh2_;
        }

        const std::size_t bankCount = RAMBankCount();

        if (bankCount == 0) {
            return 0xFF;
        }

        ramBank %= bankCount;

        const std::size_t offset =
            ramBank * RAM_BANK_SIZE +
            static_cast<std::size_t>(address - 0xA000);

        if (offset >= ram.size()) {
            return 0xFF;
        }

        return ram[offset];
    }

    return 0xFF;
}

auto Cartridge::Write(
    const uint16_t address,
    const uint8_t value
) -> void {
    if (!Loaded()) {
        return;
    }

    if (address <= 0x7FFF) {
        if (mapper_ == Mapper::MBC1) {
            WriteMBC1(address, value);
        }

        return;
    }

    if (0xA000 <= address && address <= 0xBFFF) {
        if (mapper_ != Mapper::MBC1 ||
            !ramEnabled_ ||
            ram.empty()) {
            return;
        }

        std::size_t ramBank = 0;

        if (bankingMode_ == 1) {
            ramBank = bankHigh2_;
        }

        const std::size_t bankCount = RAMBankCount();

        if (bankCount == 0) {
            return;
        }

        ramBank %= bankCount;

        const std::size_t offset =
            ramBank * RAM_BANK_SIZE +
            static_cast<std::size_t>(address - 0xA000);

        if (offset < ram.size()) {
            ram[offset] = value;
        }
    }
}

auto Cartridge::Loaded() const noexcept -> bool {
    return !rom.empty();
}

auto Cartridge::Size() const noexcept -> std::size_t {
    return rom.size();
}

auto Cartridge::Header() const noexcept
-> const CartridgeHeader& {
    return cartridgeHeader;
}

auto Cartridge::ParseHeader() -> void {
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
        DecodeROMSize(
            cartridgeHeader.romSizeCode
        );

    cartridgeHeader.declaredRamSize =
        DecodeRAMSize(
            cartridgeHeader.ramSizeCode
        );

    cartridgeHeader.headerChecksumValid =
        CalculateHeaderChecksum()
        ==
        cartridgeHeader.headerChecksum;
}

auto Cartridge::ConfigureMapper() -> void {
    ram.clear();

    switch (cartridgeHeader.type) {
    case 0x00:
        mapper_ = Mapper::ROMOnly;
        break;

    case 0x01:
        mapper_ = Mapper::MBC1;
        break;

    case 0x02:
    case 0x03:
        mapper_ = Mapper::MBC1;
        ram.resize(cartridgeHeader.declaredRamSize, 0x00);
        break;

    default:
        throw std::runtime_error(
            std::format(
                "Unsupported cartridge type: {} (0x{:02X})",
                CartridgeTypeName(cartridgeHeader.type),
                static_cast<unsigned>(cartridgeHeader.type)
            )
        );
    }
}

auto Cartridge::ResetMapperState() noexcept -> void {
    ramEnabled_ = false;
    romBankLow5_ = 1;
    bankHigh2_ = 0;
    bankingMode_ = 0;
}

auto Cartridge::ReadROMOnly(
    const uint16_t address
) const -> uint8_t {
    if (address >= rom.size()) {
        return 0xFF;
    }

    return rom[address];
}

auto Cartridge::ReadMBC1(
    const uint16_t address
) const -> uint8_t {
    if (address <= 0x3FFF) {
        std::size_t bank = 0;

        // In MBC1 mode 1, the upper two bank bits also select the
        // 0000-3FFF region. This matters for ROMs larger than 512 KiB.
        if (bankingMode_ == 1) {
            bank = static_cast<std::size_t>(bankHigh2_) << 5u;
        }

        return ReadROMBank(bank, address);
    }

    uint8_t low5 = static_cast<uint8_t>(romBankLow5_ & 0x1Fu);

    // MBC1 cannot select banks 00, 20, 40 or 60 in the switchable
    // region. A zero low-bank value is translated to one.
    if (low5 == 0) {
        low5 = 1;
    }

    const std::size_t bank =
        (static_cast<std::size_t>(bankHigh2_ & 0x03u) << 5u) |
        low5;

    return ReadROMBank(
        bank,
        static_cast<uint16_t>(address - 0x4000)
    );
}

auto Cartridge::WriteMBC1(
    const uint16_t address,
    const uint8_t value
) -> void {
    if (address <= 0x1FFF) {
        // Only the low nibble is significant. 0x0A enables RAM.
        ramEnabled_ = (value & 0x0Fu) == 0x0Au;
        return;
    }

    if (address <= 0x3FFF) {
        romBankLow5_ = static_cast<uint8_t>(value & 0x1Fu);
        return;
    }

    if (address <= 0x5FFF) {
        bankHigh2_ = static_cast<uint8_t>(value & 0x03u);
        return;
    }

    bankingMode_ = static_cast<uint8_t>(value & 0x01u);
}

auto Cartridge::ReadROMBank(
    std::size_t bank,
    const uint16_t bankAddress
) const -> uint8_t {
    const std::size_t bankCount = ROMBankCount();

    if (bankCount == 0) {
        return 0xFF;
    }

    bank %= bankCount;

    const std::size_t offset =
        bank * ROM_BANK_SIZE +
        static_cast<std::size_t>(bankAddress);

    if (offset >= rom.size()) {
        return 0xFF;
    }

    return rom[offset];
}

auto Cartridge::ROMBankCount() const noexcept
-> std::size_t {
    if (rom.empty()) {
        return 0;
    }

    return (
        rom.size() + ROM_BANK_SIZE - 1
    ) / ROM_BANK_SIZE;
}

auto Cartridge::RAMBankCount() const noexcept
-> std::size_t {
    if (ram.empty()) {
        return 0;
    }

    return (
        ram.size() + RAM_BANK_SIZE - 1
    ) / RAM_BANK_SIZE;
}

auto Cartridge::CalculateHeaderChecksum() const
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

auto Cartridge::DecodeROMSize(uint8_t code)
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

auto Cartridge::DecodeRAMSize(uint8_t code)
-> std::size_t {
    switch (code) {
    case 0x00:
        return 0;

    case 0x01:
        return 2 * 1024;

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

auto Cartridge::CartridgeTypeName(uint8_t type)
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

auto Cartridge::PrintInfo(std::ostream& os) const -> void {
    if (!Loaded()) {
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
        CartridgeTypeName(cartridgeHeader.type),
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

} // namespace PixelLink::GameBoy
