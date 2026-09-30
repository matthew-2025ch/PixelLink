#include <algorithm>
#include <fstream>
#include <format>
#include <iterator>
#include <stdexcept>
#include <array>
#include <chrono>
#include <cstdint>

#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/GameBoy/Mapper/ROMOnly.hpp>
#include <PixelLink/GameBoy/Mapper/MBC1.hpp>
#include <PixelLink/GameBoy/Mapper/MBC3.hpp>
#include <PixelLink/GameBoy/Mapper/MBC5.hpp>

namespace PixelLink::GameBoy {

namespace {

constexpr std::array<uint8_t, 6> RTC_MAGIC{
    'P', 'L', 'R', 'T', 'C', '1'
};
// RTC file: six-byte magic, five register bytes, then a little-endian
// Unix timestamp. The .sav file contains raw cartridge RAM.

auto HasRTC(uint8_t type) -> bool {
    return type == 0x0F || type == 0x10;
}

auto HasBattery(uint8_t type) -> bool {
    switch (type) {
    case 0x03:
    case 0x0F:
    case 0x10:
    case 0x13:
    case 0x1B:
    case 0x1E:
        return true;
    default:
        return false;
    }
}

auto UnixSeconds() -> int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

auto WriteBytes(
    const std::filesystem::path& path,
    const uint8_t* bytes,
    std::size_t count
) -> void {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open save file: " + path.string());
    }
    file.write(
        reinterpret_cast<const char*>(bytes),
        static_cast<std::streamsize>(count)
    );
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write save file: " + path.string());
    }
}

} // namespace

Cartridge::~Cartridge() noexcept {
    try {
        Save();
    } catch (...) {
        // Destructors cannot report I/O errors; callers can use Save().
    }
}

auto Cartridge::Load(
    const std::filesystem::path& path
) -> void
{
    Save();

    const auto absolutePath = std::filesystem::absolute(path);

    std::ifstream file(absolutePath, std::ios::binary);

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

    savePath_ = absolutePath;
    savePath_.replace_extension(".sav");
    rtcPath_ = absolutePath;
    rtcPath_.replace_extension(".rtc");
    batteryBacked_ = HasBattery(cartridgeHeader.type);
    saveDirty_ = false;

    try {
        LoadSave();
    } catch (...) {
        batteryBacked_ = false;
        throw;
    }
}

auto Cartridge::LoadSave() -> void {
    if (!batteryBacked_) {
        return;
    }

    if (!ram.empty() && std::filesystem::exists(savePath_)) {
        std::ifstream file(savePath_, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open save file: " + savePath_.string());
        }
        if (std::filesystem::file_size(savePath_) != ram.size()) {
            throw std::runtime_error("Invalid save size: " + savePath_.string());
        }
        file.read(
            reinterpret_cast<char*>(ram.data()),
            static_cast<std::streamsize>(ram.size())
        );
        if (!file) {
            throw std::runtime_error("Failed to read save file: " + savePath_.string());
        }
    }

    if (!HasRTC(cartridgeHeader.type) ||
        !std::filesystem::exists(rtcPath_)) {
        return;
    }

    std::ifstream file(rtcPath_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open RTC file: " + rtcPath_.string());
    }
    std::array<uint8_t, 19> bytes{};
    file.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!file || file.peek() != std::char_traits<char>::eof() ||
        !std::equal(RTC_MAGIC.begin(), RTC_MAGIC.end(), bytes.begin())) {
        throw std::runtime_error("Invalid RTC file: " + rtcPath_.string());
    }

    MBC3::RTCRegisters registers{};
    std::copy_n(bytes.begin() + 6, registers.size(), registers.begin());
    if (registers[0] >= 60 || registers[1] >= 60 ||
        registers[2] >= 24 || (registers[4] & 0x3E) != 0) {
        throw std::runtime_error("Invalid RTC state: " + rtcPath_.string());
    }

    uint64_t savedAt = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        savedAt |= static_cast<uint64_t>(bytes[11 + i]) << (8 * i);
    }
    const int64_t now = UnixSeconds();
    const uint64_t elapsed =
        savedAt <= static_cast<uint64_t>(now)
            ? static_cast<uint64_t>(now) - savedAt
            : 0;
    static_cast<MBC3*>(mapper_.get())->RestoreRTC(registers, elapsed);
}

auto Cartridge::Save() -> void {
    if (!batteryBacked_ || !mapper_) {
        return;
    }

    if (saveDirty_ && !ram.empty()) {
        WriteBytes(savePath_, ram.data(), ram.size());
    }

    if (HasRTC(cartridgeHeader.type)) {
        const auto registers =
            static_cast<MBC3*>(mapper_.get())->SnapshotRTC();
        std::array<uint8_t, 19> bytes{};
        std::copy(RTC_MAGIC.begin(), RTC_MAGIC.end(), bytes.begin());
        std::copy(registers.begin(), registers.end(), bytes.begin() + 6);
        const auto savedAt = static_cast<uint64_t>(UnixSeconds());
        for (std::size_t i = 0; i < 8; ++i) {
            bytes[11 + i] = static_cast<uint8_t>(savedAt >> (8 * i));
        }
        WriteBytes(rtcPath_, bytes.data(), bytes.size());
    }

    saveDirty_ = false;
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
        if (batteryBacked_) {
            saveDirty_ = true;
        }
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
                ram,
                HasRTC(cartridgeHeader.type)
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
