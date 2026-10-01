#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/TestUtils.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::CoreTimingTest {

namespace {

void testLDHReadsScanlineDuringThirdMachineCycle() {
    ::PixelLink::GameBoy::GameBoy gameBoy;
    auto& bus = gameBoy.GetBus();
    auto& ppu = gameBoy.GetPPU();
    auto& cpu = gameBoy.GetCPU();

    Test::Load(bus, 0x0100, {
        0xF0, 0x44, // LDH A,(LY), 12 T-cycles
    });

    // LY changes from 143 to 144 eight dots into this instruction.
    ppu.Step(143u * 456u + 448u);
    CHECK(ppu.GetLY() == 143);
    CHECK(ppu.GetLineDot() == 448);

    CHECK(gameBoy.Step() == 12);
    CHECK(cpu.A == 144);
    CHECK(cpu.PC == 0x0102);
    CHECK(ppu.GetLY() == 144);
    CHECK(ppu.GetLineDot() == 4);
}

} // namespace

void run() {
    Test::run(
        "Core timing / LDH reads current scanline",
        testLDHReadsScanlineDuringThirdMachineCycle
    );
}

} // namespace PixelLink::Test::GameBoy::CoreTimingTest
