#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/CPU.hpp>
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

void testMemoryAccessCycles() {
    Bus bus;
    CPU cpu(bus);
    Test::Load(bus, 0x0100, {
        0x7E,             // LD A,(HL): read at T=4
        0xFA, 0x00, 0xC0, // LD A,(C000): read at T=12
        0x34,             // INC (HL): read at T=4, write at T=8
    });
    cpu.H = 0xC0;
    cpu.L = 0;
    std::uint32_t elapsed = 0;
    cpu.SetCycleCallback([&](const std::uint32_t cycles) {
        elapsed += cycles;
        if (cpu.PC <= 0x0104) {
            bus.Write(0xC000, static_cast<uint8_t>(elapsed));
        } else if (elapsed == 8) {
            CHECK(bus.Read(0xC000) == 0x20); // read has not written yet
            bus.Write(0xC000, 0x80); // the RMW must retain its earlier read
        } else if (elapsed == 12) {
            CHECK(bus.Read(0xC000) == 0x21);
        }
    });
    CHECK(cpu.Step() == 8);
    CHECK(elapsed == 8);
    CHECK(cpu.A == 4);
    elapsed = 0;
    CHECK(cpu.Step() == 16);
    CHECK(elapsed == 16);
    CHECK(cpu.A == 12);
    elapsed = 0;
    bus.Write(0xC000, 0x20);
    CHECK(cpu.Step() == 12);
    CHECK(elapsed == 12);
    CHECK(bus.Read(0xC000) == 0x21);
}

void testStackAndInterruptCycles() {
    Bus bus;
    CPU cpu(bus);
    Test::Load(bus, 0x0100, {0xCD, 0x00, 0x02}); // CALL 0200
    cpu.SP = 0xC100;
    std::uint32_t elapsed = 0;
    cpu.SetCycleCallback([&](const std::uint32_t cycles) {
        elapsed += cycles;
        CHECK(bus.Read(0xC0FF) == (elapsed >= 20 ? 1 : 0));
        CHECK(bus.Read(0xC0FE) == (elapsed >= 24 ? 3 : 0));
    });
    CHECK(cpu.Step() == 24);
    CHECK(elapsed == 24);
    CHECK(cpu.PC == 0x0200);

    // Interrupt entry has two idle M-cycles, then high/low stack writes,
    // then the final vector-selection cycle.
    cpu.PC = 0x1234;
    cpu.SP = 0xC100;
    cpu.ime = true;
    bus.Write(0xC0FF, 0);
    bus.Write(0xC0FE, 0);
    bus.Write(0xFFFF, 1);
    bus.Write(0xFF0F, 1);
    elapsed = 0;
    cpu.SetCycleCallback([&](const std::uint32_t cycles) {
        elapsed += cycles;
        CHECK(bus.Read(0xC0FF) == (elapsed >= 12 ? 0x12 : 0));
        CHECK(bus.Read(0xC0FE) == (elapsed >= 16 ? 0x34 : 0));
    });
    CHECK(cpu.Step() == 20);
    CHECK(elapsed == 20);
    CHECK(cpu.PC == 0x0040);
}

} // namespace

void run() {
    Test::run("Core timing / memory machine cycles", testMemoryAccessCycles);
    Test::run("Core timing / stack and interrupt machine cycles", testStackAndInterruptCycles);
    Test::run(
        "Core timing / LDH reads current scanline",
        testLDHReadsScanlineDuringThirdMachineCycle
    );
}

} // namespace PixelLink::Test::GameBoy::CoreTimingTest
