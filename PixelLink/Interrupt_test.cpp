#include <cstdint>
#include <format>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "Bus.hpp"
#include "CPU.hpp"

namespace {

#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            throw std::runtime_error(                                 \
                std::format(                                         \
                    "CHECK failed: {} at {}:{}",                      \
                    #expr,                                            \
                    __FILE__,                                         \
                    __LINE__                                          \
                )                                                     \
            );                                                        \
        }                                                             \
    } while (false)

    constexpr uint16_t IF = 0xFF0F;
    constexpr uint16_t IE = 0xFFFF;

    void load(
        Bus& bus,
        uint16_t address,
        std::initializer_list<uint8_t> bytes
    ) {
        for (const uint8_t byte : bytes) {
            bus.write(address++, byte);
        }
    }

    template <typename Func>
    void runTest(
        std::string_view name,
        Func test
    ) {
        try {
            test();

            std::cout
                << "[PASS] "
                << name
                << '\n';
        }
        catch (const std::exception& e) {
            std::cerr
                << "[FAIL] "
                << name
                << '\n'
                << "       "
                << e.what()
                << '\n';

            throw;
        }
    }


    // ============================================================
    // 1. EI delay
    // ============================================================

    void testEIDelay() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0xFB, // EI
            0x00, // NOP
            0x00  // should NOT execute before interrupt
            });

        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        CHECK(cpu.ime == false);

        // EI
        CHECK(cpu.step() == 4);

        CHECK(cpu.PC == 0x0101);
        CHECK(cpu.ime == false);

        // The NOP after EI must execute first
        CHECK(cpu.step() == 4);

        CHECK(cpu.PC == 0x0102);
        CHECK(cpu.ime == true);

        // Only now is the interrupt entered
        CHECK(cpu.step() == 20);

        CHECK(cpu.PC == 0x0040);
        CHECK(cpu.ime == false);
    }


    // ============================================================
    // 2. VBlank interrupt
    // ============================================================

    void testVBlankInterrupt() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0xFB, // EI
            0x00  // NOP
            });

        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        cpu.step(); // EI
        cpu.step(); // NOP

        CHECK(cpu.PC == 0x0102);
        CHECK(cpu.SP == 0xFFFE);

        CHECK(cpu.step() == 20);

        // VBlank vector
        CHECK(cpu.PC == 0x0040);

        // PC=0102 was pushed
        CHECK(cpu.SP == 0xFFFC);

        CHECK(bus.read(0xFFFC) == 0x02);
        CHECK(bus.read(0xFFFD) == 0x01);

        // IF bit 0 was cleared by the CPU
        CHECK(
            (bus.read(IF) & 0x01) == 0
        );

        // Interrupt entry automatically disables IME
        CHECK(cpu.ime == false);
    }


    // ============================================================
    // 3. Interrupt priority
    // ============================================================

    void testInterruptPriority() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0xFB,
            0x00
            });

        // Enable all
        bus.write(IE, 0x1F);

        // Request:
        //
        // VBlank bit 0
        // Timer  bit 2
        // Joypad bit 4

        bus.write(
            IF,
            static_cast<uint8_t>(
                (1u << 0)
                | (1u << 2)
                | (1u << 4)
                )
        );

        cpu.step(); // EI
        cpu.step(); // NOP

        CHECK(cpu.step() == 20);

        // bit 0 must take priority
        CHECK(cpu.PC == 0x0040);

        // bit 0 was cleared
        CHECK(
            (bus.read(IF) & 0x01) == 0
        );

        // Timer is still pending
        CHECK(
            (bus.read(IF) & 0x04) != 0
        );

        // Joypad is still pending
        CHECK(
            (bus.read(IF) & 0x10) != 0
        );
    }


    // ============================================================
    // 4. IME false => interrupt waits
    // ============================================================

    void testInterruptWaitsWhenIMEDisabled() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0x00, // NOP
            0x00
            });

        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        CHECK(cpu.ime == false);

        // Must NOT enter 0040
        CHECK(cpu.step() == 4);

        CHECK(cpu.PC == 0x0101);

        // IF is still retained
        CHECK(
            (bus.read(IF) & 0x01) != 0
        );
    }


    // ============================================================
    // 5. HALT wake-up with IME=false
    // ============================================================

    void testHaltWakeWithoutIME() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0x76, // HALT
            0x00  // NOP
            });

        // Execute HALT
        CHECK(cpu.step() == 4);

        CHECK(cpu.halted);
        CHECK(cpu.PC == 0x0101);

        // No interrupt, keep halting
        CHECK(cpu.step() == 4);

        CHECK(cpu.halted);
        CHECK(cpu.PC == 0x0101);

        // A VBlank interrupt now appears
        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        // IME=false:
        //
        // Should wake up,
        // but must not jump to 0040.
        //
        // So the next NOP executes normally.

        CHECK(cpu.step() == 4);

        CHECK(!cpu.halted);
        CHECK(cpu.PC == 0x0102);

        // The interrupt was not serviced,
        // so IF is still set.
        CHECK(
            (bus.read(IF) & 0x01) != 0
        );
    }


    // ============================================================
    // 6. DI cancels delayed EI
    // ============================================================

    void testDICancelsEI() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0xFB, // EI
            0xF3, // DI
            0x00  // NOP
            });

        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        // EI
        CHECK(cpu.step() == 4);

        CHECK(cpu.ime == false);

        // DI
        CHECK(cpu.step() == 4);

        CHECK(cpu.ime == false);

        // If the EI delay were not cancelled by DI,
        // this would incorrectly enter 0040.
        CHECK(cpu.step() == 4);

        CHECK(cpu.PC == 0x0103);

        CHECK(cpu.ime == false);
    }


    // ============================================================
    // 7. RETI
    // ============================================================

    void testRETI() {
        Bus bus;
        CPU cpu(bus);

        load(bus, 0x0100, {
            0xFB, // EI
            0x00  // NOP
            });

        // Interrupt handler:
        //
        // 0040:
        // RETI

        load(bus, 0x0040, {
            0xD9
            });

        bus.write(IE, 0x01);
        bus.write(IF, 0x01);

        cpu.step(); // EI
        cpu.step(); // NOP

        CHECK(cpu.step() == 20);

        CHECK(cpu.PC == 0x0040);
        CHECK(cpu.ime == false);

        CHECK(cpu.step() == 16);

        // Return to the interrupted PC
        CHECK(cpu.PC == 0x0102);

        CHECK(cpu.SP == 0xFFFE);

        // RETI re-enables IME
        CHECK(cpu.ime == true);
    }


    // ============================================================
    // 8. All interrupt vectors
    // ============================================================

    void testInterruptVectors() {
        struct TestCase {
            uint8_t bit;
            uint16_t vector;
        };

        constexpr TestCase cases[] = {
            { 0, 0x0040 },
            { 1, 0x0048 },
            { 2, 0x0050 },
            { 3, 0x0058 },
            { 4, 0x0060 }
        };

        for (const auto& test : cases) {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0xFB,
                0x00
                });

            const uint8_t mask =
                static_cast<uint8_t>(
                    1u << test.bit
                    );

            bus.write(IE, mask);
            bus.write(IF, mask);

            cpu.step(); // EI
            cpu.step(); // NOP

            CHECK(cpu.step() == 20);

            CHECK(cpu.PC == test.vector);

            CHECK(
                (bus.read(IF) & mask) == 0
            );
        }
    }

} // namespace


int main() {
    try {
        runTest(
            "EI delay",
            testEIDelay
        );

        runTest(
            "VBlank interrupt",
            testVBlankInterrupt
        );

        runTest(
            "interrupt priority",
            testInterruptPriority
        );

        runTest(
            "IME disabled",
            testInterruptWaitsWhenIMEDisabled
        );

        runTest(
            "HALT wake-up",
            testHaltWakeWithoutIME
        );

        runTest(
            "DI cancels EI",
            testDICancelsEI
        );

        runTest(
            "RETI",
            testRETI
        );

        runTest(
            "interrupt vectors",
            testInterruptVectors
        );
    }
    catch (...) {
        std::cerr
            << "\nInterrupt tests FAILED.\n";

        return 1;
    }

    std::cout
        << "\nAll interrupt tests passed.\n";

    return 0;
}