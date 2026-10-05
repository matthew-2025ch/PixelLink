#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include <PixelLink/Test/GameBoy/MapperTestUtils.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::SaveTest {
namespace {

class TestRom {
public:
    TestRom(
        const char* name,
        uint8_t type,
        uint8_t ramSize
    ) : path_(name) {
        savePath_ = path_;
        savePath_.replace_extension(".sav");
        rtcPath_ = path_;
        rtcPath_.replace_extension(".rtc");

        std::error_code error;
        std::filesystem::remove(savePath_, error);
        std::filesystem::remove(rtcPath_, error);
        MapperTestUtils::CreateROM(path_, type, 0x01, ramSize);
    }

    ~TestRom() {
        std::error_code error;
        std::filesystem::remove(path_, error);
        std::filesystem::remove(savePath_, error);
        std::filesystem::remove(rtcPath_, error);
    }

    TestRom(const TestRom&) = delete;
    TestRom& operator=(const TestRom&) = delete;

    const auto& path() const { return path_; }
    const auto& savePath() const { return savePath_; }
    const auto& rtcPath() const { return rtcPath_; }

private:
    std::filesystem::path path_;
    std::filesystem::path savePath_;
    std::filesystem::path rtcPath_;
};

auto EnableRAM(Cartridge& cartridge) -> void {
    cartridge.Write(0x0000, 0x0A);
}

auto WriteRTC(Cartridge& cartridge, uint8_t reg, uint8_t value) -> void {
    cartridge.Write(0x4000, reg);
    cartridge.Write(0xA000, value);
}

auto ReadRTC(Cartridge& cartridge, uint8_t reg) -> uint8_t {
    cartridge.Write(0x4000, reg);
    return cartridge.Read(0xA000);
}

auto BackdateRTC(
    const std::filesystem::path& path,
    uint64_t seconds
) -> void {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("Failed to open RTC test file");
    }

    std::array<uint8_t, 8> stamp{};
    file.seekg(11);
    file.read(reinterpret_cast<char*>(stamp.data()), stamp.size());
    if (!file) {
        throw std::runtime_error("Failed to read RTC timestamp");
    }

    uint64_t value = 0;
    for (std::size_t i = 0; i < stamp.size(); ++i) {
        value |= static_cast<uint64_t>(stamp[i]) << (8 * i);
    }
    value -= seconds;
    for (std::size_t i = 0; i < stamp.size(); ++i) {
        stamp[i] = static_cast<uint8_t>(value >> (8 * i));
    }
    file.seekp(11);
    file.write(reinterpret_cast<const char*>(stamp.data()), stamp.size());
    if (!file) {
        throw std::runtime_error("Failed to update RTC timestamp");
    }
}

void testMBC1BatteryRAM() {
    TestRom rom("save_mbc1_test.gb", 0x03, 0x03);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        cartridge.Write(0xA000, 0x2A);
        cartridge.Write(0x6000, 1);
        cartridge.Write(0x4000, 2);
        cartridge.Write(0xA000, 0x7C);
    }

    CHECK(std::filesystem::file_size(rom.savePath()) == 32 * 1024);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        CHECK(cartridge.Read(0xA000) == 0x2A);
        cartridge.Write(0x6000, 1);
        cartridge.Write(0x4000, 2);
        CHECK(cartridge.Read(0xA000) == 0x7C);
    }
}

void testMBC3AndMBC5BatteryRAM() {
    for (const auto [name, type] : {
        std::pair{"save_mbc3_test.gb", uint8_t{0x13}},
        std::pair{"save_mbc5_test.gb", uint8_t{0x1B}}
    }) {
        TestRom rom(name, type, 0x03);
        {
            Cartridge cartridge;
            cartridge.Load(rom.path());
            EnableRAM(cartridge);
            cartridge.Write(0x4000, 3);
            cartridge.Write(0xA123, 0xB6);
            cartridge.Save();
        }
        CHECK(std::filesystem::exists(rom.savePath()));
        {
            Cartridge cartridge;
            cartridge.Load(rom.path());
            EnableRAM(cartridge);
            cartridge.Write(0x4000, 3);
            CHECK(cartridge.Read(0xA123) == 0xB6);
        }
    }
}

void testNonBatteryDoesNotSave() {
    TestRom rom("save_no_battery_test.gb", 0x02, 0x02);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        cartridge.Write(0xA000, 0x55);
        cartridge.Save();
    }
    CHECK(!std::filesystem::exists(rom.savePath()));
}

void testSwitchROMSavesPrevious() {
    TestRom first("save_switch_first.gb", 0x03, 0x02);
    TestRom second("save_switch_second.gb", 0x03, 0x02);
    Cartridge cartridge;
    cartridge.Load(first.path());
    EnableRAM(cartridge);
    cartridge.Write(0xA000, 0x39);
    cartridge.Load(second.path());
    CHECK(std::filesystem::exists(first.savePath()));

    Cartridge restored;
    restored.Load(first.path());
    EnableRAM(restored);
    CHECK(restored.Read(0xA000) == 0x39);
}

void testInvalidRAMSaveIsRejected() {
    TestRom rom("save_invalid_test.gb", 0x03, 0x02);
    {
        std::ofstream file(rom.savePath(), std::ios::binary);
        file.put('x');
    }
    bool rejected = false;
    try {
        Cartridge cartridge;
        cartridge.Load(rom.path());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    CHECK(rejected);
    CHECK(std::filesystem::file_size(rom.savePath()) == 1);
}

void testHaltedRTCDoesNotAdvanceOffline() {
    TestRom rom("save_halted_rtc_test.gb", 0x0F, 0x00);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        WriteRTC(cartridge, 0x0C, 0x40);
        WriteRTC(cartridge, 0x08, 17);
        WriteRTC(cartridge, 0x09, 23);
        WriteRTC(cartridge, 0x0A, 7);
        cartridge.Save();
    }
    CHECK(std::filesystem::exists(rom.rtcPath()));
    CHECK(!std::filesystem::exists(rom.savePath()));
    BackdateRTC(rom.rtcPath(), 3600);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        CHECK(ReadRTC(cartridge, 0x08) == 17);
        CHECK(ReadRTC(cartridge, 0x09) == 23);
        CHECK(ReadRTC(cartridge, 0x0A) == 7);
        CHECK((ReadRTC(cartridge, 0x0C) & 0x40) != 0);
    }
}

void testRunningRTCCatchesUpOffline() {
    TestRom rom("save_running_rtc_test.gb", 0x10, 0x03);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        WriteRTC(cartridge, 0x0C, 0x40);
        WriteRTC(cartridge, 0x08, 59);
        WriteRTC(cartridge, 0x09, 59);
        WriteRTC(cartridge, 0x0A, 23);
        WriteRTC(cartridge, 0x0B, 0xFF);
        WriteRTC(cartridge, 0x0C, 0x41);
        WriteRTC(cartridge, 0x0C, 0x01);
        cartridge.Save();
    }
    BackdateRTC(rom.rtcPath(), 3);
    {
        Cartridge cartridge;
        cartridge.Load(rom.path());
        EnableRAM(cartridge);
        CHECK(ReadRTC(cartridge, 0x0B) == 0);
        const uint8_t dayHigh = ReadRTC(cartridge, 0x0C);
        CHECK((dayHigh & 0x01) == 0);
        CHECK((dayHigh & 0x80) != 0);
        CHECK(ReadRTC(cartridge, 0x08) >= 2);
    }
}

void testInvalidRTCFileIsRejected() {
    TestRom rom("save_invalid_rtc_test.gb", 0x0F, 0x00);
    {
        std::ofstream file(rom.rtcPath(), std::ios::binary);
        file.put('x');
    }
    bool rejected = false;
    try {
        Cartridge cartridge;
        cartridge.Load(rom.path());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    CHECK(rejected);
    CHECK(std::filesystem::file_size(rom.rtcPath()) == 1);
}

} // namespace

void run() {
    Test::run("Save / MBC1 battery RAM", testMBC1BatteryRAM);
    Test::run("Save / MBC3 and MBC5 battery RAM", testMBC3AndMBC5BatteryRAM);
    Test::run("Save / No battery", testNonBatteryDoesNotSave);
    Test::run("Save / ROM switch", testSwitchROMSavesPrevious);
    Test::run("Save / Invalid RAM size", testInvalidRAMSaveIsRejected);
    Test::run("Save / Halted RTC", testHaltedRTCDoesNotAdvanceOffline);
    Test::run("Save / Running RTC", testRunningRTCCatchesUpOffline);
    Test::run("Save / Invalid RTC file", testInvalidRTCFileIsRejected);
}

} // namespace PixelLink::Test::GameBoy::SaveTest
