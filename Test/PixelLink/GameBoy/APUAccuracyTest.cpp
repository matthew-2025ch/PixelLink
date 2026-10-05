#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <chrono>

namespace PixelLink::Test::GameBoy::APUAccuracyTest {
namespace {
constexpr std::array NAMES{
    "01-registers", "02-len ctr", "03-trigger", "04-sweep",
    "05-sweep details", "06-overflow on trigger", "07-len sweep period sync",
    "08-len ctr during power", "09-wave read while on", "10-wave trigger while on",
    "11-regs after power", "12-wave write while on"
};

class TestROM {
public:
    explicit TestROM(const std::filesystem::path& original) {
        const auto root = std::filesystem::path(PIXELLINK_APU_TEST_DIR);
        directory_ = root / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        if (!std::filesystem::create_directory(directory_)) throw std::runtime_error("Cannot create fresh APU fixture");
        path = directory_ / original.filename();
        try { std::filesystem::copy_file(original, path); }
        catch (...) { std::filesystem::remove(directory_); throw; }
    }
    ~TestROM() {
        // Remove only our three known generated files, never ROM library saves.
        std::error_code ignored;
        auto save = path; save.replace_extension(".sav");
        auto rtc = path; rtc.replace_extension(".rtc");
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(save, ignored);
        std::filesystem::remove(rtc, ignored);
        std::filesystem::remove(directory_, ignored);
    }
    std::filesystem::path path;
private:
    std::filesystem::path directory_;
};

void RunROM(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Missing APU test ROM: " + path.string());
    TestROM fixture(path);
    PixelLink::GameBoy::GameBoy gb;
    gb.LoadROM(fixture.path);
    auto& bus = gb.GetBus();
    const auto read = [&](std::uint16_t address) {
        return bus.Read(address, PixelLink::GameBoy::BusAccess::Internal);
    };
    std::uint64_t cycles = 0;
    constexpr std::uint64_t budget = 30ull * 4'194'304;
    for (std::uint64_t steps = 0; cycles < budget; ++steps) {
        const int elapsed = gb.Step();
        if (elapsed <= 0) throw std::runtime_error("CPU stopped during APU test");
        cycles += elapsed;
        if ((steps & 1023) != 0) continue;
        // Blargg's documented memory result protocol. ROMs and the emulated
        // CPU/hardware are unmodified; this only observes the final result.
        if (read(0xA001) != 0xDE || read(0xA002) != 0xB0 || read(0xA003) != 0x61 || read(0xA000) >= 0x80) continue;
        if (read(0xA000) == 0) return;
        std::string output;
        for (std::uint16_t address = 0xA004; address < 0xC000 && read(address); ++address) {
            output += static_cast<char>(read(address));
        }
        throw std::runtime_error(std::format("Blargg result={}, cycles={}\n{}", read(0xA000), cycles, output));
    }
    throw std::runtime_error(std::format("APU ROM timed out after {} cycles at PC={:04X}",
        cycles, gb.GetCPU().GetRegisterSnapshot().pc));
}
}

void run() {
    const auto directory = std::filesystem::path(PIXELLINK_PROJECT_DIR) / "assests/roms/blargg-dmg-sound";
    unsigned failures = 0;
    for (const auto* name : NAMES) {
        try {
            Test::run(std::format("APU accuracy / Blargg {}", name), [&] { RunROM(directory / std::format("{}.gb", name)); });
        } catch (const std::exception&) { ++failures; }
    }
    if (failures) throw std::runtime_error(std::format("{} of {} APU ROMs failed", failures, NAMES.size()));
}
}
