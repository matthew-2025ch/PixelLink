#include <chrono>
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

        case Mapper::MBC3:
            return ReadMBC3(address);
        }
    }

    if (0xA000 <= address && address <= 0xBFFF) {
        switch (mapper_) {
        case Mapper::ROMOnly:
            return 0xFF;

        case Mapper::MBC1:
            return ReadMBC1RAM(address);

        case Mapper::MBC3:
            return ReadMBC3RAMRTC(address);
        }
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
        switch (mapper_) {
        case Mapper::ROMOnly:
            break;

        case Mapper::MBC1:
            WriteMBC1(address, value);
            break;

        case Mapper::MBC3:
            WriteMBC3(address, value);
            break;
        }

        return;
    }

    if (0xA000 <= address && address <= 0xBFFF) {
        switch (mapper_) {
        case Mapper::ROMOnly:
            break;

        case Mapper::MBC1:
            WriteMBC1RAM(address, value);
            break;

        case Mapper::MBC3:
            WriteMBC3RAMRTC(address, value);
            break;
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
    hasRTC_ = false;

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

    case 0x0F:
        mapper_ = Mapper::MBC3;
        hasRTC_ = true;
        break;

    case 0x10:
        mapper_ = Mapper::MBC3;
        hasRTC_ = true;
        ram.resize(cartridgeHeader.declaredRamSize, 0x00);
        break;

    case 0x11:
        mapper_ = Mapper::MBC3;
        break;

    case 0x12:
    case 0x13:
        mapper_ = Mapper::MBC3;
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

    mbc3RomBank_ = 1;
    mbc3RamRtcSelect_ = 0;
    mbc3LastLatchWrite_ = 0xFF;

    rtc_ = {};
    latchedRTC_.fill(0);
    rtcLatchedValid_ = false;
    rtcLastUpdate_ = std::chrono::steady_clock::now();
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

auto Cartridge::ReadMBC1RAM(
    const uint16_t address
) const -> uint8_t {
    if (!ramEnabled_ || ram.empty()) {
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

auto Cartridge::WriteMBC1RAM(
    const uint16_t address,
    const uint8_t value
) -> void {
    if (!ramEnabled_ || ram.empty()) {
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

auto Cartridge::ReadMBC3(
    const uint16_t address
) const -> uint8_t {
    if (address <= 0x3FFF) {
        return ReadROMBank(0, address);
    }

    std::size_t bank =
        static_cast<std::size_t>(mbc3RomBank_ & 0x7Fu);

    if (bank == 0) {
        bank = 1;
    }

    return ReadROMBank(
        bank,
        static_cast<uint16_t>(address - 0x4000)
    );
}

auto Cartridge::ReadMBC3RAMRTC(
    const uint16_t address
) const -> uint8_t {
    if (!ramEnabled_) {
        return 0xFF;
    }

    if (mbc3RamRtcSelect_ <= 0x07u) {
        if (ram.empty()) {
            return 0xFF;
        }

        const std::size_t bankCount = RAMBankCount();

        if (bankCount == 0) {
            return 0xFF;
        }

        const std::size_t ramBank =
            static_cast<std::size_t>(mbc3RamRtcSelect_) %
            bankCount;

        const std::size_t offset =
            ramBank * RAM_BANK_SIZE +
            static_cast<std::size_t>(address - 0xA000);

        if (offset >= ram.size()) {
            return 0xFF;
        }

        return ram[offset];
    }

    if (hasRTC_ &&
        0x08u <= mbc3RamRtcSelect_ &&
        mbc3RamRtcSelect_ <= 0x0Cu) {
        return ReadRTCRegister(mbc3RamRtcSelect_);
    }

    return 0xFF;
}

auto Cartridge::WriteMBC3(
    const uint16_t address,
    const uint8_t value
) -> void {
    if (address <= 0x1FFF) {
        // MBC3 uses the same RAM/RTC enable pattern as MBC1.
        ramEnabled_ = (value & 0x0Fu) == 0x0Au;
        return;
    }

    if (address <= 0x3FFF) {
        mbc3RomBank_ =
            static_cast<uint8_t>(value & 0x7Fu);

        // Bank 00 is remapped to bank 01. Unlike MBC1, banks
        // 20, 40 and 60 are valid on MBC3.
        if (mbc3RomBank_ == 0) {
            mbc3RomBank_ = 1;
        }

        return;
    }

    if (address <= 0x5FFF) {
        // 00-07 select RAM banks, 08-0C select RTC registers.
        // Other values leave A000-BFFF unmapped.
        mbc3RamRtcSelect_ = value;
        return;
    }

    // A 00 -> 01 transition latches the current RTC state.
    if (mbc3LastLatchWrite_ == 0x00u &&
        value == 0x01u &&
        hasRTC_) {
        LatchRTC();
    }

    mbc3LastLatchWrite_ = value;
}

auto Cartridge::WriteMBC3RAMRTC(
    const uint16_t address,
    const uint8_t value
) -> void {
    if (!ramEnabled_) {
        return;
    }

    if (mbc3RamRtcSelect_ <= 0x07u) {
        if (ram.empty()) {
            return;
        }

        const std::size_t bankCount = RAMBankCount();

        if (bankCount == 0) {
            return;
        }

        const std::size_t ramBank =
            static_cast<std::size_t>(mbc3RamRtcSelect_) %
            bankCount;

        const std::size_t offset =
            ramBank * RAM_BANK_SIZE +
            static_cast<std::size_t>(address - 0xA000);

        if (offset < ram.size()) {
            ram[offset] = value;
        }

        return;
    }

    if (hasRTC_ &&
        0x08u <= mbc3RamRtcSelect_ &&
        mbc3RamRtcSelect_ <= 0x0Cu) {
        WriteRTCRegister(mbc3RamRtcSelect_, value);
    }
}

auto Cartridge::SyncRTC() const -> void {
    if (!hasRTC_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    if (rtc_.halt) {
        rtcLastUpdate_ = now;
        return;
    }

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(
            now - rtcLastUpdate_
        ).count();

    if (elapsed <= 0) {
        return;
    }

    rtcLastUpdate_ += std::chrono::seconds(elapsed);

    std::uint64_t totalSeconds =
        static_cast<std::uint64_t>(rtc_.seconds) +
        static_cast<std::uint64_t>(rtc_.minutes) * 60u +
        static_cast<std::uint64_t>(rtc_.hours) * 60u * 60u +
        static_cast<std::uint64_t>(rtc_.days) * 24u * 60u * 60u +
        static_cast<std::uint64_t>(elapsed);

    const std::uint64_t totalDays =
        totalSeconds / (24u * 60u * 60u);

    if (totalDays >= 512u) {
        rtc_.carry = true;
    }

    rtc_.days =
        static_cast<uint16_t>(totalDays & 0x01FFu);

    totalSeconds %= 24u * 60u * 60u;

    rtc_.hours =
        static_cast<uint8_t>(
            totalSeconds / (60u * 60u)
        );

    totalSeconds %= 60u * 60u;

    rtc_.minutes =
        static_cast<uint8_t>(
            totalSeconds / 60u
        );

    rtc_.seconds =
        static_cast<uint8_t>(
            totalSeconds % 60u
        );
}

auto Cartridge::LatchRTC() const -> void {
    SyncRTC();

    for (uint8_t reg = 0x08; reg <= 0x0C; ++reg) {
        latchedRTC_[reg - 0x08] =
            RTCRegisterValue(reg);
    }

    rtcLatchedValid_ = true;
}

auto Cartridge::ReadRTCRegister(
    const uint8_t reg
) const -> uint8_t {
    if (!hasRTC_ || reg < 0x08u || reg > 0x0Cu) {
        return 0xFF;
    }

    if (rtcLatchedValid_) {
        return latchedRTC_[reg - 0x08u];
    }

    SyncRTC();
    return RTCRegisterValue(reg);
}

auto Cartridge::WriteRTCRegister(
    const uint8_t reg,
    const uint8_t value
) -> void {
    if (!hasRTC_ || reg < 0x08u || reg > 0x0Cu) {
        return;
    }

    SyncRTC();

    switch (reg) {
    case 0x08:
        rtc_.seconds =
            static_cast<uint8_t>((value & 0x3Fu) % 60u);
        break;

    case 0x09:
        rtc_.minutes =
            static_cast<uint8_t>((value & 0x3Fu) % 60u);
        break;

    case 0x0A:
        rtc_.hours =
            static_cast<uint8_t>((value & 0x1Fu) % 24u);
        break;

    case 0x0B:
        rtc_.days =
            static_cast<uint16_t>(
                (rtc_.days & 0x0100u) |
                static_cast<uint16_t>(value)
            );
        break;

    case 0x0C: {
        const bool wasHalted = rtc_.halt;

        rtc_.days =
            static_cast<uint16_t>(
                (rtc_.days & 0x00FFu) |
                (static_cast<uint16_t>(value & 0x01u) << 8u)
            );

        rtc_.halt = (value & 0x40u) != 0;
        rtc_.carry = (value & 0x80u) != 0;

        // Reset the host-time anchor whenever the halt state changes so
        // time spent halted is never added when the clock resumes.
        if (wasHalted != rtc_.halt) {
            rtcLastUpdate_ = std::chrono::steady_clock::now();
        }

        break;
    }

    default:
        break;
    }
}

auto Cartridge::RTCRegisterValue(
    const uint8_t reg
) const -> uint8_t {
    switch (reg) {
    case 0x08:
        return rtc_.seconds;

    case 0x09:
        return rtc_.minutes;

    case 0x0A:
        return rtc_.hours;

    case 0x0B:
        return static_cast<uint8_t>(rtc_.days & 0x00FFu);

    case 0x0C: {
        uint8_t value =
            static_cast<uint8_t>((rtc_.days >> 8u) & 0x01u);

        if (rtc_.halt) {
            value = static_cast<uint8_t>(value | 0x40u);
        }

        if (rtc_.carry) {
            value = static_cast<uint8_t>(value | 0x80u);
        }

        return value;
    }

    default:
        return 0xFF;
    }
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
