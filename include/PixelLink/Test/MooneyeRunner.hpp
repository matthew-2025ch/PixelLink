#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <stdexcept>

#include <PixelLink/GameBoy/GameBoy.hpp>

namespace PixelLink::Test {

// Stop at the official LD B,B result breakpoint, without patching the ROM
// or bypassing any emulated hardware. Each invocation starts a fresh DMG.
inline void RunMooneyeROM(const std::filesystem::path& romPath) {
    if (!std::filesystem::is_regular_file(romPath)) {
        throw std::runtime_error(std::format(
            "Test ROM not found: {}", romPath.string()));
    }

    PixelLink::GameBoy::GameBoy gameBoy;
    gameBoy.LoadROM(romPath);
    gameBoy.GetAPU().SetSampleCaptureEnabled(false);
    auto& cpu = gameBoy.GetCPU();
    auto& bus = gameBoy.GetBus();
    std::uint64_t cycles = 0;
    constexpr std::uint64_t maxCycles = 20ull * 4'194'304ull;

    const auto diagnostic = [&] {
        const auto r = cpu.GetRegisterSnapshot();
        auto details = std::format(
            "cycles={}, PC={:04X}, AF={:02X}{:02X}, BC={:02X}{:02X}, "
            "DE={:02X}{:02X}, HL={:02X}{:02X}, LY={}, STAT={:02X}, "
            "DIV={:02X}, TIMA={:02X}, TMA={:02X}, TAC={:02X}, IF={:02X}",
            cycles, r.pc, r.a, r.f, r.b, r.c, r.d, r.e, r.h, r.l,
            gameBoy.GetPPU().GetLY(),
            bus.Read(0xFF41, PixelLink::GameBoy::BusAccess::Internal),
            gameBoy.GetTimer().Read(0xFF04), gameBoy.GetTimer().Read(0xFF05),
            gameBoy.GetTimer().Read(0xFF06), gameBoy.GetTimer().Read(0xFF07),
            bus.Read(0xFF0F, PixelLink::GameBoy::BusAccess::Internal));
        // Mooneye saves assertion registers and custom failure data in HRAM.
        // Include these bytes without imposing a particular ROM's layout.
        details += "\nHRAM FF80-FF9F:";
        for (std::uint16_t address = 0xFF80; address < 0xFFA0; ++address) {
            details += std::format(" {:02X}",
                bus.Read(address, PixelLink::GameBoy::BusAccess::Internal));
        }
        return details;
    };

    while (cycles < maxCycles) {
        const auto r = cpu.GetRegisterSnapshot();
        if (!bus.IsOAMDMAActive() &&
            bus.Read(r.pc, PixelLink::GameBoy::BusAccess::Internal) == 0x40) {
            if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 &&
                r.h == 21 && r.l == 34) {
                return;
            }
            if (r.b == 0x42 && r.c == 0x42 && r.d == 0x42 &&
                r.e == 0x42 && r.h == 0x42 && r.l == 0x42) {
                throw std::runtime_error("ROM reported failure: " + diagnostic());
            }
        }

        const int elapsed = gameBoy.Step();
        if (elapsed <= 0) {
            throw std::runtime_error("CPU did not advance: " + diagnostic());
        }
        cycles += static_cast<std::uint64_t>(elapsed);
    }
    throw std::runtime_error("ROM timed out: " + diagnostic());
}

} // namespace PixelLink::Test
