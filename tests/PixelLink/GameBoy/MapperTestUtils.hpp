#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace PixelLink::Test::GameBoy::MapperTestUtils {

inline auto CalculateChecksum(const std::vector<uint8_t>& rom) -> uint8_t {
    uint8_t checksum = 0;

    for (uint16_t address = 0x0134; address <= 0x014C; ++address) {
        checksum = static_cast<uint8_t>(
            checksum - rom[address] - 1
        );
    }

    return checksum;
}

inline auto CreateROM(
    const std::filesystem::path& path,
    uint8_t type,
    uint8_t romSize,
    uint8_t ramSize
) -> void {
    std::vector<uint8_t> rom(64 * 1024, 0);

    rom[0x0134] = 'T';
    rom[0x0135] = 'E';
    rom[0x0136] = 'S';
    rom[0x0137] = 'T';

    rom[0x0147] = type;
    rom[0x0148] = romSize;
    rom[0x0149] = ramSize;

    rom[0x014D] = CalculateChecksum(rom);

    std::ofstream file(path, std::ios::binary);
    file.write(
        reinterpret_cast<const char*>(rom.data()),
        static_cast<std::streamsize>(rom.size())
    );
}

}
