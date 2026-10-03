#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <stdexcept>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Test::GameBoy::ROMTest {
namespace {

constexpr std::uint64_t MAX_CYCLES = 20ull * 4'194'304ull;
constexpr std::array ROM_NAMES = {
    "add_sp_e_timing",
    "div_timing",
    "ei_sequence",
    "ei_timing",
    "halt_ime0_ei",
    "halt_ime0_nointr_timing",
    "halt_ime1_timing",
    "ld_hl_sp_e_timing"
};

auto IsSuccess(
    const PixelLink::GameBoy::CPU::RegisterSnapshot& registers
) -> bool {
    return registers.b == 3 && registers.c == 5 &&
           registers.d == 8 && registers.e == 13 &&
           registers.h == 21 && registers.l == 34;
}

auto IsFailure(
    const PixelLink::GameBoy::CPU::RegisterSnapshot& registers
) -> bool {
    return registers.b == 0x42 && registers.c == 0x42 &&
           registers.d == 0x42 && registers.e == 0x42 &&
           registers.h == 0x42 && registers.l == 0x42;
}

auto RunRom(const std::filesystem::path& romPath) -> void {
    if (!std::filesystem::is_regular_file(romPath)) {
        throw std::runtime_error(std::format(
            "Test ROM not found: {}", romPath.string()));
    }

    PixelLink::GameBoy::GameBoy gameBoy;
    gameBoy.LoadROM(romPath);

    auto& cpu = gameBoy.GetCPU();
    auto& bus = gameBoy.GetBus();
    std::uint64_t cycles = 0;

    while (cycles < MAX_CYCLES) {
        const auto registers = cpu.GetRegisterSnapshot();
        // Mooneye uses LD B,B as a breakpoint after placing its result
        // signature in B/C/D/E/H/L. Detect it before executing the opcode.
        if (!bus.IsOAMDMAActive() &&
            bus.Read(registers.pc, PixelLink::GameBoy::BusAccess::Internal) == 0x40) {
            if (IsSuccess(registers)) {
                return;
            }
            if (IsFailure(registers)) {
                throw std::runtime_error("Test ROM reported failure");
            }
        }

        const int elapsed = gameBoy.Step();
        if (elapsed <= 0) {
            throw std::runtime_error("CPU did not advance");
        }
        cycles += static_cast<std::uint64_t>(elapsed);
    }

    throw std::runtime_error(std::format(
        "Test ROM timed out after {} cycles at PC=0x{:04X}",
        cycles, cpu.GetRegisterSnapshot().pc));
}

} // namespace

void run() {
    const auto romDirectory = std::filesystem::path(PIXELLINK_PROJECT_DIR) /
        "assests/roms/mooneye/acceptance";

    for (const auto* name : ROM_NAMES) {
        Test::run(std::format("ROM / Mooneye {}", name), [&] {
            RunRom(romDirectory / std::format("{}.gb", name));
        });
    }
}

} // namespace PixelLink::Test::GameBoy::ROMTest
