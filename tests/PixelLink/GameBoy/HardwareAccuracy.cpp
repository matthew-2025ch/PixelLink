#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <stdexcept>

#include <PixelLink/Test/GameBoy/MooneyeRunner.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestSuites.hpp>

namespace PixelLink::Tests::Gameboy::HardwareAccuracy {
namespace {

// Run the complete checked-in acceptance set in a stable order. An explicit
// list also makes a missing ROM fail instead of silently reducing coverage.
constexpr std::array ROM_NAMES = {
    "add_sp_e_timing",
    "div_timing",
    "ei_sequence",
    "ei_timing",
    "halt_ime0_ei",
    "halt_ime0_nointr_timing",
    "halt_ime1_timing",
    "ld_hl_sp_e_timing",
    "oam_dma_start",
    "oam_dma_restart",
    "oam_dma_timing",
    "oam_dma/basic",
    "oam_dma/reg_read",
    "oam_dma/sources-GS",
    "ppu/hblank_ly_scx_timing-GS",
    "ppu/intr_1_2_timing-GS",
    "ppu/intr_2_0_timing",
    "ppu/intr_2_mode0_timing",
    "ppu/intr_2_mode0_timing_sprites",
    "ppu/intr_2_mode3_timing",
    "ppu/intr_2_oam_ok_timing",
    "ppu/lcdon_timing-GS",
    "ppu/lcdon_write_timing-GS",
    "ppu/stat_irq_blocking",
    "ppu/stat_lyc_onoff",
    "ppu/vblank_stat_intr-GS",
    "timer/div_write",
    "timer/rapid_toggle",
    "timer/tim00",
    "timer/tim00_div_trigger",
    "timer/tim01",
    "timer/tim01_div_trigger",
    "timer/tim10",
    "timer/tim10_div_trigger",
    "timer/tim11",
    "timer/tim11_div_trigger",
    "timer/tima_reload",
    "timer/tima_write_reloading",
    "timer/tma_write_reloading",
    "serial/boot_sclk_align-dmgABCmgb",
};

} // namespace

void run() {
    const auto romDirectory = std::filesystem::path(PIXELLINK_PROJECT_DIR) /
        "assests/roms/mooneye/acceptance";
    std::size_t failures = 0;

    for (const auto* name : ROM_NAMES) {
        try {
            ::PixelLink::Test::run(
                std::format("Hardware accuracy / Mooneye {}", name),
                [&] {
                    ::PixelLink::Test::RunMooneyeROM(
                        romDirectory / std::format("{}.gb", name));
                }
            );
        } catch (const std::exception&) {
            // Test::run has printed this ROM's diagnostic. Continue through
            // the remaining cases, then fail the complete suite.
            ++failures;
        }
    }

    if (failures != 0) {
        throw std::runtime_error(std::format(
            "{} of {} Mooneye ROMs failed", failures, ROM_NAMES.size()));
    }
}

} // namespace PixelLink::Tests::Gameboy::HardwareAccuracy
