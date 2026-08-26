#include <cstdint>
#include <format>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "Bus.hpp"
#include "CPU.hpp"

namespace CPUTest {

    namespace {

        // ============================================================
        // Minimal test utilities
        // ============================================================

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

        constexpr uint8_t FLAG_Z = 0x80;
        constexpr uint8_t FLAG_N = 0x40;
        constexpr uint8_t FLAG_H = 0x20;
        constexpr uint8_t FLAG_C = 0x10;

        bool flagSet(const CPU& cpu, uint8_t flag) {
            return (cpu.F & flag) != 0;
        }

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
        void runTest(std::string_view name, Func test) {
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
        // 1. LD / ADD / Flags
        // ============================================================

        void testLoadAddFlags() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x3E, 0x0F, // LD A,$0F
                0x06, 0x01, // LD B,$01

                0x80,       // ADD A,B
                // 0F + 01 = 10
                // H = 1

    0xC6, 0xF0, // ADD A,$F0
    // 10 + F0 = 100
    // A = 00
    // Z = 1
    // C = 1
    // H = 0

    0x76        // HALT
                });

            CHECK(cpu.step() == 8);
            CHECK(cpu.A == 0x0F);

            CHECK(cpu.step() == 8);
            CHECK(cpu.B == 0x01);

            CHECK(cpu.step() == 4);

            CHECK(cpu.A == 0x10);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(!flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 8);

            CHECK(cpu.A == 0x00);

            CHECK(flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 4);
            CHECK(cpu.halted);
        }


        // ============================================================
        // 2. INC / DEC / Carry preservation
        // ============================================================

        void testIncDec() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x37,       // SCF -> C = 1

                0x06, 0x0F, // LD B,$0F

                0x04,       // INC B
                // 0F -> 10
                // H=1
                // C must stay 1

    0x05,       // DEC B
    // 10 -> 0F
    // N=1 H=1
    // C must stay 1

    0x76
                });

            CHECK(cpu.step() == 4);
            CHECK(flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 8);
            CHECK(cpu.B == 0x0F);

            CHECK(cpu.step() == 4);

            CHECK(cpu.B == 0x10);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 4);

            CHECK(cpu.B == 0x0F);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));

            // INC / DEC must not modify Carry
            CHECK(flagSet(cpu, FLAG_C));
        }


        // ============================================================
        // 3. ADC / SBC
        // ============================================================

        void testAdcSbc() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x3E, 0xFF, // LD A,$FF

                0x37,       // SCF -> C = 1

                0xCE, 0x00, // ADC A,$00
                // FF + 00 + 1 = 100
                // A = 00
                // Z H C = 1

    0x3E, 0x00, // LD A,$00

    0x37,       // SCF

    0xDE, 0x00, // SBC A,$00
    // 00 - 00 - 1 = FF
    // N H C = 1

    0x76
                });

            cpu.step();
            cpu.step();

            CHECK(cpu.step() == 8);

            CHECK(cpu.A == 0x00);

            CHECK(flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));

            cpu.step();
            cpu.step();

            CHECK(cpu.step() == 8);

            CHECK(cpu.A == 0xFF);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));
        }


        // ============================================================
        // 4. SUB / CP
        // ============================================================

        void testSubAndCp() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x3E, 0x10, // LD A,$10

                0xD6, 0x01, // SUB $01
                // 10 - 01 = 0F
                // N=1 H=1

    0xFE, 0x0F, // CP $0F
    // equal
    // Z=1
    // A must remain 0F

    0x76
                });

            cpu.step();

            CHECK(cpu.step() == 8);

            CHECK(cpu.A == 0x0F);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(!flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 8);

            // CP must not modify A
            CHECK(cpu.A == 0x0F);

            CHECK(flagSet(cpu, FLAG_Z));
            CHECK(flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(!flagSet(cpu, FLAG_C));
        }


        // ============================================================
        // 5. Conditional JR
        // ============================================================

        void testConditionalJump() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x3E, 0x08, // 0100 LD A,$08

                0xFE, 0x08, // 0102 CP $08
                // Z = 1

    0x28, 0x02, // 0104 JR Z,+2
    // taken -> 0108

    0x3E, 0xFF, // 0106 skipped

    0xFE, 0x07, // 0108 CP $07
    // Z = 0

    0x28, 0x02, // 010A JR Z,+2
    // NOT taken

    0x3E, 0x42, // 010C LD A,$42

    0x76        // 010E HALT
                });

            cpu.step();
            cpu.step();

            CHECK(flagSet(cpu, FLAG_Z));

            // taken
            CHECK(cpu.step() == 12);
            CHECK(cpu.PC == 0x0108);

            cpu.step();

            CHECK(!flagSet(cpu, FLAG_Z));

            // not taken
            CHECK(cpu.step() == 8);
            CHECK(cpu.PC == 0x010C);

            cpu.step();

            CHECK(cpu.A == 0x42);
        }


        // ============================================================
        // 6. CALL / RET / stack byte order
        // ============================================================

        void testCallReturn() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0xCD, 0x00, 0x02, // CALL $0200

                0x3E, 0x42,       // LD A,$42

                0x76              // HALT
                });

            load(bus, 0x0200, {
                0x06, 0x99, // LD B,$99
                0xC9        // RET
                });

            CHECK(cpu.step() == 24);

            CHECK(cpu.PC == 0x0200);
            CHECK(cpu.SP == 0xFFFC);

            // CALL's return address = 0103
            //
            // Stack:
            //
            // FFFC = low  = 03
            // FFFD = high = 01

            CHECK(bus.read(0xFFFC) == 0x03);
            CHECK(bus.read(0xFFFD) == 0x01);

            CHECK(cpu.step() == 8);
            CHECK(cpu.B == 0x99);

            CHECK(cpu.step() == 16);

            CHECK(cpu.PC == 0x0103);
            CHECK(cpu.SP == 0xFFFE);

            CHECK(cpu.step() == 8);
            CHECK(cpu.A == 0x42);

            CHECK(cpu.step() == 4);
            CHECK(cpu.halted);
        }


        // ============================================================
        // 7. PUSH / POP BC
        // ============================================================

        void testPushPop() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x01, 0x34, 0x12, // LD BC,$1234

                0xC5,             // PUSH BC

                0x01, 0x00, 0x00, // LD BC,$0000

                0xC1,             // POP BC

                0x76
                });

            CHECK(cpu.step() == 12);

            CHECK(cpu.B == 0x12);
            CHECK(cpu.C == 0x34);

            CHECK(cpu.step() == 16);

            CHECK(cpu.SP == 0xFFFC);

            CHECK(bus.read(0xFFFC) == 0x34);
            CHECK(bus.read(0xFFFD) == 0x12);

            CHECK(cpu.step() == 12);

            CHECK(cpu.B == 0x00);
            CHECK(cpu.C == 0x00);

            CHECK(cpu.step() == 12);

            CHECK(cpu.B == 0x12);
            CHECK(cpu.C == 0x34);

            CHECK(cpu.SP == 0xFFFE);
        }


        // ============================================================
        // 8. POP AF low nibble must be cleared
        // ============================================================

        void testPopAFMask() {
            Bus bus;
            CPU cpu(bus);

            // Prepare the stack manually
            //
            // POP AF:
            //
            // low  -> F
            // high -> A

            bus.write(0xC000, 0xFF);
            bus.write(0xC001, 0x12);

            load(bus, 0x0100, {
                0x31, 0x00, 0xC0, // LD SP,$C000

                0xF1,             // POP AF

                0xF5,             // PUSH AF

                0x76
                });

            CHECK(cpu.step() == 12);

            CHECK(cpu.SP == 0xC000);

            CHECK(cpu.step() == 12);

            CHECK(cpu.A == 0x12);

            // Even though the input is FF,
            // F's low nibble must be cleared to 0.
            CHECK(cpu.F == 0xF0);

            CHECK(cpu.SP == 0xC002);

            CHECK(cpu.step() == 16);

            CHECK(cpu.SP == 0xC000);

            CHECK(bus.read(0xC000) == 0xF0);
            CHECK(bus.read(0xC001) == 0x12);
        }


        // ============================================================
        // 9. CB instructions + (HL)
        // ============================================================

        void testMemoryAndCB() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x21, 0x00, 0xC0, // LD HL,$C000

                0x36, 0x80,       // LD (HL),$80

                0xCB, 0x06,       // RLC (HL)
                // 80 -> 01
                // C=1

    0xCB, 0x46,       // BIT 0,(HL)
    // bit 0 = 1
    // Z=0
    // H=1
    // C preserved

    0xCB, 0x86,       // RES 0,(HL)
    // 01 -> 00

    0xCB, 0xC6,       // SET 0,(HL)
    // 00 -> 01

    0x7E,             // LD A,(HL)

    0x76
                });

            CHECK(cpu.step() == 12);

            CHECK(cpu.H == 0xC0);
            CHECK(cpu.L == 0x00);

            CHECK(cpu.step() == 12);

            CHECK(bus.read(0xC000) == 0x80);

            CHECK(cpu.step() == 16);

            CHECK(bus.read(0xC000) == 0x01);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));

            // BIT 0,(HL)
            CHECK(cpu.step() == 12);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));

            // BIT must not modify C
            CHECK(flagSet(cpu, FLAG_C));

            // RES
            CHECK(cpu.step() == 16);

            CHECK(bus.read(0xC000) == 0x00);

            // RES must not change flags
            CHECK(flagSet(cpu, FLAG_C));

            // SET
            CHECK(cpu.step() == 16);

            CHECK(bus.read(0xC000) == 0x01);

            cpu.step();

            CHECK(cpu.A == 0x01);
        }


        // ============================================================
        // 10. ADD HL,r16
        // ============================================================

        void testAddHL() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x3E, 0x00,       // LD A,$00

                0xFE, 0x00,       // CP $00
                // Z=1

    0x21, 0xFF, 0x0F, // LD HL,$0FFF

    0x01, 0x01, 0x00, // LD BC,$0001

    0x09,             // ADD HL,BC
    // 0FFF + 0001 = 1000
    // H=1

    0x01, 0x00, 0xF0, // LD BC,$F000

    0x09,             // ADD HL,BC
    // 1000 + F000 = 0000
    // C=1

    0x76
                });

            cpu.step();
            cpu.step();

            CHECK(flagSet(cpu, FLAG_Z));

            cpu.step();
            cpu.step();

            CHECK(cpu.step() == 8);

            CHECK(cpu.H == 0x10);
            CHECK(cpu.L == 0x00);

            // ADD HL must not modify Z
            CHECK(flagSet(cpu, FLAG_Z));

            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(!flagSet(cpu, FLAG_C));

            cpu.step();

            CHECK(cpu.step() == 8);

            CHECK(cpu.H == 0x00);
            CHECK(cpu.L == 0x00);

            // Z is still preserved
            CHECK(flagSet(cpu, FLAG_Z));

            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));
        }


        // ============================================================
        // 11. ADD SP,e8 / LD HL,SP+e8
        // ============================================================

        void testSignedSP() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                0x31, 0xF8, 0xFF, // LD SP,$FFF8

                0xE8, 0x08,       // ADD SP,+8
                // -> 0000
                // H=1 C=1
                // Z=N=0

    0x31, 0x08, 0x00, // LD SP,$0008

    0xF8, 0xF8,       // LD HL,SP-8
    // -> 0000
    // H=1 C=1

    0x76
                });

            CHECK(cpu.step() == 12);

            CHECK(cpu.SP == 0xFFF8);

            CHECK(cpu.step() == 16);

            CHECK(cpu.SP == 0x0000);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));

            CHECK(cpu.step() == 12);

            CHECK(cpu.SP == 0x0008);

            CHECK(cpu.step() == 12);

            CHECK(cpu.H == 0x00);
            CHECK(cpu.L == 0x00);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));
        }


        // ============================================================
        // 12. DAA
        // ============================================================

        void testDAA() {
            Bus bus;
            CPU cpu(bus);

            load(bus, 0x0100, {
                // BCD:
                // 15 + 27 = 42

                0x3E, 0x15, // LD A,$15

                0xC6, 0x27, // ADD A,$27
                // binary result = 3C

    0x27,       // DAA
    // -> 42

    // BCD:
    // 88 + 99 = 187
    //
    // result = 87
    // carry = 1

    0x3E, 0x88,

    0xC6, 0x99,

    0x27,

    0x76
                });

            cpu.step();
            cpu.step();

            CHECK(cpu.A == 0x3C);

            CHECK(cpu.step() == 4);

            CHECK(cpu.A == 0x42);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(!flagSet(cpu, FLAG_C));

            cpu.step();
            cpu.step();

            CHECK(cpu.step() == 4);

            CHECK(cpu.A == 0x87);

            CHECK(!flagSet(cpu, FLAG_Z));
            CHECK(!flagSet(cpu, FLAG_N));
            CHECK(!flagSet(cpu, FLAG_H));
            CHECK(flagSet(cpu, FLAG_C));
        }

    } // namespace


    // ============================================================
    // Main
    // ============================================================

    void CPUTest() {
        try {
            runTest(
                "LD / ADD / flags",
                testLoadAddFlags
            );

            runTest(
                "INC / DEC",
                testIncDec
            );

            runTest(
                "ADC / SBC",
                testAdcSbc
            );

            runTest(
                "SUB / CP",
                testSubAndCp
            );

            runTest(
                "conditional JR",
                testConditionalJump
            );

            runTest(
                "CALL / RET",
                testCallReturn
            );

            runTest(
                "PUSH / POP",
                testPushPop
            );

            runTest(
                "POP AF mask",
                testPopAFMask
            );

            runTest(
                "CB / memory",
                testMemoryAndCB
            );

            runTest(
                "ADD HL",
                testAddHL
            );

            runTest(
                "signed SP",
                testSignedSP
            );

            runTest(
                "DAA",
                testDAA
            );
        }
        catch (...) {
            std::cerr
                << "\nCPU tests FAILED.\n";
            return;
        }


        std::cout
            << "\nAll CPU tests passed.\n";
    }

} //namespace CPUTest