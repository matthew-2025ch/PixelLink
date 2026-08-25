#pragma once

#include <cstdint>
#include <stdexcept>
#include <format>
#include "Bus.hpp"

class CPU {
public:
    explicit CPU(Bus& bus);

    auto reset() -> void;
    auto step() -> int;

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

    auto fetch8() -> uint8_t;
    auto fetch16() -> uint16_t;
    auto execute(uint8_t opcode) -> int;
    auto executeCB(uint8_t opcode) -> int;

    auto getFlag(Flag flag) const -> bool;
    auto setFlag(Flag flag, bool value) -> void;
    auto setZNHC(bool z, bool n, bool h, bool c) -> void;

    auto getBC() const -> uint16_t;
    auto getDE() const -> uint16_t;
    auto getHL() const -> uint16_t;
    auto setBC(uint16_t value) -> void;
    auto setDE(uint16_t value) -> void;
    auto setHL(uint16_t value) -> void;

    auto readR8(uint8_t code) -> uint8_t;
    auto writeR8(uint8_t code, uint8_t value) -> void;

    auto push16(uint16_t value) -> void;
    auto pop16() -> uint16_t;

    auto inc8(uint8_t value) -> uint8_t;
    auto dec8(uint8_t value) -> uint8_t;
    auto addA(uint8_t value, bool withCarry) -> void;
    auto subA(uint8_t value, bool withCarry) -> void;
    auto cpA(uint8_t value) -> void;
    auto andA(uint8_t value) -> void;
    auto xorA(uint8_t value) -> void;
    auto orA(uint8_t value) -> void;
    auto addHL(uint16_t value) -> void;
    auto addSignedToSP(int8_t offset) -> uint16_t;

    auto relativeJump(bool condition) -> int;
    auto conditionalJP(bool condition) -> int;
    auto conditionalCall(bool condition) -> int;
    auto conditionalRet(bool condition) -> int;
    auto restart(uint16_t vector) -> int;
};
