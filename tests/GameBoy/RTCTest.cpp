#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::RTCTest {

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
    const std::filesystem::path& path,
    const uint8_t cartridgeType
) -> void {
    std::vector<uint8_t> rom(
        32 * 1024,
        0
    );

    constexpr char title[] = "RTCTEST";

    for (
        std::size_t i = 0;
        i < sizeof(title) - 1;
        ++i
    ) {
        rom[0x0134 + i] =
            static_cast<uint8_t>(title[i]);
    }

    rom[0x0147] = cartridgeType;
    rom[0x0148] = 0x00; // 32 KiB ROM.
    rom[0x0149] = 0x03; // 32 KiB RAM.

    rom[0x014D] =
        calculateHeaderChecksum(rom);

    std::ofstream file(
        path,
        std::ios::binary
    );

    if (!file) {
        throw std::runtime_error(
            "Failed to create RTC test ROM"
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

    if (!file) {
        throw std::runtime_error(
            "Failed to write RTC test ROM"
        );
    }
}

class TempRom {
public:
    TempRom(
        std::filesystem::path path,
        const uint8_t cartridgeType
    )
        : path_(std::move(path)) {
        createTestRom(
            path_,
            cartridgeType
        );
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

auto enableRTC(
    Cartridge& cartridge
) -> void {
    cartridge.Write(
        0x0000,
        0x0A
    );
}

auto disableRTC(
    Cartridge& cartridge
) -> void {
    cartridge.Write(
        0x0000,
        0x00
    );
}

auto selectRTCRegister(
    Cartridge& cartridge,
    const uint8_t reg
) -> void {
    cartridge.Write(
        0x4000,
        reg
    );
}

auto writeRTCRegister(
    Cartridge& cartridge,
    const uint8_t reg,
    const uint8_t value
) -> void {
    selectRTCRegister(
        cartridge,
        reg
    );

    cartridge.Write(
        0xA000,
        value
    );
}

auto readRTCRegister(
    Cartridge& cartridge,
    const uint8_t reg
) -> uint8_t {
    selectRTCRegister(
        cartridge,
        reg
    );

    return cartridge.Read(
        0xA000
    );
}

auto latchRTC(
    Cartridge& cartridge
) -> void {
    cartridge.Write(
        0x6000,
        0x00
    );

    cartridge.Write(
        0x6000,
        0x01
    );
}

void testRTCRegisterAccess() {
    TempRom rom(
        "rtc_test.gb",
        0x10
    );

    Cartridge cartridge;
    cartridge.Load(rom.path());

    selectRTCRegister(
        cartridge,
        0x08
    );

    CHECK(
        cartridge.Read(0xA000)
        == 0xFF
    );

    enableRTC(cartridge);

    // Halt the clock so the values stay deterministic.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x40
    );

    writeRTCRegister(
        cartridge,
        0x08,
        12
    );

    writeRTCRegister(
        cartridge,
        0x09,
        34
    );

    writeRTCRegister(
        cartridge,
        0x0A,
        5
    );

    writeRTCRegister(
        cartridge,
        0x0B,
        0x34
    );

    // Day bit 8 = 1 and Halt = 1.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x41
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        == 12
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x09
        )
        == 34
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x0A
        )
        == 5
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x0B
        )
        == 0x34
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x0C
        )
        == 0x41
    );

    disableRTC(cartridge);

    selectRTCRegister(
        cartridge,
        0x08
    );

    CHECK(
        cartridge.Read(0xA000)
        == 0xFF
    );
}

void testRTCLatch() {
    TempRom rom(
        "rtc_test.gb",
        0x10
    );

    Cartridge cartridge;
    cartridge.Load(rom.path());

    enableRTC(cartridge);

    // Halt the clock so only explicit writes change seconds.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x40
    );

    writeRTCRegister(
        cartridge,
        0x08,
        7
    );

    // A lone write of 1 must not latch.
    cartridge.Write(
        0x6000,
        0x01
    );

    writeRTCRegister(
        cartridge,
        0x08,
        9
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        == 9
    );

    // Only a 0 -> 1 transition captures the RTC snapshot.
    latchRTC(cartridge);

    writeRTCRegister(
        cartridge,
        0x08,
        11
    );

    // Reads still expose the old latched snapshot.
    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        == 9
    );

    // A new latch replaces the snapshot.
    latchRTC(cartridge);

    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        == 11
    );
}

void testRTCHaltAndResume() {
    TempRom rom(
        "rtc_test.gb",
        0x10
    );

    Cartridge cartridge;
    cartridge.Load(rom.path());

    enableRTC(cartridge);

    writeRTCRegister(
        cartridge,
        0x08,
        20
    );

    writeRTCRegister(
        cartridge,
        0x0C,
        0x40
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1200)
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        == 20
    );

    CHECK(
        (
            readRTCRegister(
                cartridge,
                0x0C
            )
            & 0x40
        )
        != 0
    );

    // Resume the clock.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x00
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1200)
    );

    CHECK(
        readRTCRegister(
            cartridge,
            0x08
        )
        != 20
    );
}

void testRTCDayOverflowAndCarry() {
    TempRom rom(
        "rtc_test.gb",
        0x10
    );

    Cartridge cartridge;
    cartridge.Load(rom.path());

    enableRTC(cartridge);

    // Start at day 511, 23:59:59 while halted.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x40
    );

    writeRTCRegister(
        cartridge,
        0x08,
        59
    );

    writeRTCRegister(
        cartridge,
        0x09,
        59
    );

    writeRTCRegister(
        cartridge,
        0x0A,
        23
    );

    writeRTCRegister(
        cartridge,
        0x0B,
        0xFF
    );

    // Day bit 8 = 1 and Halt = 1.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x41
    );

    // Clear Halt while keeping day bit 8 set.
    writeRTCRegister(
        cartridge,
        0x0C,
        0x01
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1200)
    );

    const uint8_t dayLow =
        readRTCRegister(
            cartridge,
            0x0B
        );

    const uint8_t dayHigh =
        readRTCRegister(
            cartridge,
            0x0C
        );

    CHECK(dayLow == 0x00);

    CHECK(
        (dayHigh & 0x01)
        == 0
    );

    CHECK(
        (dayHigh & 0x80)
        != 0
    );
}

void testMBC3WithoutTimerHasNoRTC() {
    TempRom rom(
        "rtc_test.gb",
        0x13
    );

    Cartridge cartridge;
    cartridge.Load(rom.path());

    enableRTC(cartridge);

    selectRTCRegister(
        cartridge,
        0x08
    );

    cartridge.Write(
        0xA000,
        42
    );

    CHECK(
        cartridge.Read(0xA000)
        == 0xFF
    );
}

} // namespace

void run() {
    Test::run(
        "RTC / Register access",
        testRTCRegisterAccess
    );

    Test::run(
        "RTC / Latch",
        testRTCLatch
    );

    Test::run(
        "RTC / Halt and resume",
        testRTCHaltAndResume
    );

    Test::run(
        "RTC / Day overflow and carry",
        testRTCDayOverflowAndCarry
    );

    Test::run(
        "RTC / MBC3 without timer",
        testMBC3WithoutTimerHasNoRTC
    );
}

} // namespace PixelLink::Test::GameBoy::RTCTest
