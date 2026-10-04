#pragma once

#include <cstdint>
#include <stdexcept>
#include <format>
#include <functional>

namespace PixelLink::GameBoy {

class Bus;

class CPU {
public:
    struct RegisterSnapshot {
        std::uint8_t a, f, b, c, d, e, h, l;
        std::uint16_t pc;
    };

    explicit CPU(Bus& bus);

    auto Reset() -> void;
    auto Step() -> int;
    auto SetCycleCallback(std::function<void(std::uint32_t)> callback) -> void;
    [[nodiscard]] auto GetRegisterSnapshot() const noexcept -> RegisterSnapshot;

#ifndef _DEBUG
private:
#endif
    uint8_t A = 0;
    uint8_t F = 0;
    uint8_t B = 0;
    uint8_t C = 0;
    uint8_t D = 0;
    uint8_t E = 0;
    uint8_t H = 0;
    uint8_t L = 0;
    uint16_t SP = 0xFFFE;
    uint16_t PC = 0x0100;
    bool ime = false;
    bool halted = false;
    bool haltBugPending = false;
    uint8_t imeEnableDelay = 0;

#ifdef _DEBUG
private:
#endif
    enum Flag : uint8_t {
        Z  = 1u << 7,
        N  = 1u << 6,
        HF = 1u << 5,
        CF = 1u << 4
    };

    Bus& bus;
    std::function<void(std::uint32_t)> cycleCallback_;
    std::uint32_t stepCycles_ = 0;

    auto AdvanceCycles(std::uint32_t cycles) -> void;
    auto ReadCycle(uint16_t address) -> uint8_t;
    auto WriteCycle(uint16_t address, uint8_t value) -> void;

    auto Fetch8() -> uint8_t;
    auto FetchOpcode() -> uint8_t;
    auto Fetch16() -> uint16_t;
    auto Execute(uint8_t opcode) -> int;
    auto ExecuteCB(uint8_t opcode) -> int;

    auto GetFlag(Flag flag) const -> bool;
    auto SetFlag(Flag flag, bool value) -> void;
    auto SetZNHC(bool z, bool n, bool h, bool c) -> void;

    auto GetBC() const -> uint16_t;
    auto GetDE() const -> uint16_t;
    auto GetHL() const -> uint16_t;
    auto SetBC(uint16_t value) -> void;
    auto SetDE(uint16_t value) -> void;
    auto SetHL(uint16_t value) -> void;

    auto ReadR8(uint8_t code) -> uint8_t;
    auto WriteR8(uint8_t code, uint8_t value) -> void;

    auto Push16(uint16_t value, bool idleBefore = true) -> void;
    auto Pop16() -> uint16_t;

    auto Inc8(uint8_t value) -> uint8_t;
    auto Dec8(uint8_t value) -> uint8_t;
    auto AddA(uint8_t value, bool withCarry) -> void;
    auto SubA(uint8_t value, bool withCarry) -> void;
    auto CpA(uint8_t value) -> void;
    auto AndA(uint8_t value) -> void;
    auto XorA(uint8_t value) -> void;
    auto OrA(uint8_t value) -> void;
    auto AddHL(uint16_t value) -> void;
    auto AddSignedToSP(int8_t offset) -> uint16_t;

    auto RelativeJump(bool condition) -> int;
    auto ConditionalJP(bool condition) -> int;
    auto ConditionalCall(bool condition) -> int;
    auto ConditionalRet(bool condition) -> int;
    auto Restart(uint16_t vector) -> int;

    auto PendingInterrupts() -> uint8_t;
    auto ServiceInterrupt(uint8_t pending) -> int;
    auto UpdateIME() -> void;
};

} // namespace PixelLink::GameBoy
