#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include "Bus.hpp"
#include "Cartridge.hpp"
#include "TestFramework.hpp"
#include "TestSuites.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::CartridgeTest {

namespace {

auto calculateHeaderChecksum(
    const std::vector<uint8_t>& rom
) -> uint8_t {
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

auto createTestRom(
    const std::filesystem::path& path
) -> void {
    std::vector<uint8_t> rom(
        32 * 1024,
        0
    );

    rom[0x0100] = 0x00;
    rom[0x0101] = 0xC3;
    rom[0x0102] = 0x50;
    rom[0x0103] = 0x01;

    constexpr char title[] = "TESTROM";

    for (
        std::size_t i = 0;
        i < sizeof(title) - 1;
        ++i
    ) {
        rom[0x0134 + i] =
            static_cast<uint8_t>(title[i]);
    }

    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x00;

    rom[0x014D] =
        calculateHeaderChecksum(rom);

    rom[0x0150] = 0x3E;
    rom[0x0151] = 0x42;

    std::ofstream file(
        path,
        std::ios::binary
    );

    if (!file) {
        throw std::runtime_error(
            "Failed to create test ROM"
        );
    }

    file.write(
        reinterpret_cast<const char*>(
            rom.data()
        ),
        static_cast<std::streamsize>(
            rom.size()
        )
    );
}

class TempRom {
public:
    explicit TempRom(
        std::filesystem::path path
    )
        : path_(std::move(path)) {
        createTestRom(path_);
    }

    ~TempRom() {
        std::error_code error;
        std::filesystem::remove(
            path_,
            error
        );
    }

    TempRom(const TempRom&) = delete;
    TempRom& operator=(const TempRom&) = delete;

    auto path() const
        -> const std::filesystem::path& {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void testCartridgeLoad() {
    TempRom rom("test_rom.gb");

    Cartridge cartridge;
    cartridge.Load(rom.path());

    CHECK(cartridge.Loaded());
    CHECK(cartridge.Size() == 32 * 1024);

    CHECK(
        cartridge.Header().title
        == "TESTROM"
    );

    CHECK(
        cartridge.Header().type
        == 0x00
    );

    CHECK(
        cartridge.Header().romSizeCode
        == 0x00
    );

    CHECK(
        cartridge.Header().ramSizeCode
        == 0x00
    );

    CHECK(
        cartridge.Header().declaredRomSize
        == 32 * 1024
    );

    CHECK(
        cartridge.Header().declaredRamSize
        == 0
    );

    CHECK(
        cartridge.Header().headerChecksumValid
    );

    CHECK(cartridge.Read(0x0100) == 0x00);
    CHECK(cartridge.Read(0x0101) == 0xC3);
    CHECK(cartridge.Read(0x0150) == 0x3E);
    CHECK(cartridge.Read(0x0151) == 0x42);
}

void testBusCartridgeMapping() {
    TempRom rom("test_rom.gb");

    Cartridge cartridge;
    cartridge.Load(rom.path());

    Bus bus;

    bus.Write(
        0xC000,
        0x55
    );

    bus.InsertCartridge(
        cartridge
    );

    CHECK(bus.Read(0x0150) == 0x3E);
    CHECK(bus.Read(0x0151) == 0x42);
    CHECK(bus.Read(0xC000) == 0x55);

    bus.Write(
        0x0150,
        0xFF
    );

    CHECK(bus.Read(0x0150) == 0x3E);
}

} // namespace

void run() {
    Test::run(
        "Cartridge / Load",
        testCartridgeLoad
    );

    Test::run(
        "Cartridge / Bus mapping",
        testBusCartridgeMapping
    );
}

} // namespace PixelLink::Test::CartridgeTest
