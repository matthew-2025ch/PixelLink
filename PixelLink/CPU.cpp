#include "CPU.hpp"

CPU::CPU(Bus& bus) : bus(bus) {
    reset();
}

auto CPU::reset() -> void {
    A = 0x01;
    F = 0xB0;
    B = 0x00;
    C = 0x13;
    D = 0x00;
    E = 0xD8;
    H = 0x01;
    L = 0x4D;
    SP = 0xFFFE;
    PC = 0x0100;
    ime = false;
    imeEnableDelay = 0;
    halted = false;
}

auto CPU::step() -> int {
    const uint8_t pending =
        pendingInterrupts();

    // --------------------------------------------
    // A pending interrupt wakes up HALT
    // even when IME = false
    // --------------------------------------------

    if (pending != 0) {
        halted = false;

        if (ime) {
            return serviceInterrupt(pending);
        }
    }

    // --------------------------------------------
    // No pending interrupt yet,
    // the CPU stays halted
    // --------------------------------------------

    if (halted) {
        return 4;
    }

    // --------------------------------------------
    // Execute one opcode normally
    // --------------------------------------------

    const int cycles =
        execute(fetch8());

    updateIME();

    return cycles;
}

auto CPU::fetch8() -> uint8_t {
    return bus.read(PC++);
}

auto CPU::fetch16() -> uint16_t {
    const uint8_t low = fetch8();
    const uint8_t high = fetch8();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

auto CPU::getFlag(Flag flag) const -> bool {
    return (F & static_cast<uint8_t>(flag)) != 0;
}

auto CPU::setFlag(Flag flag, bool value) -> void {
    const uint8_t mask = static_cast<uint8_t>(flag);
    if (value) {
        F = static_cast<uint8_t>(F | mask);
    } else {
        F = static_cast<uint8_t>(F & static_cast<uint8_t>(~mask));
    }
    F &= 0xF0;
}

auto CPU::setZNHC(bool z, bool n, bool h, bool c) -> void {
    setFlag(Z, z);
    setFlag(N, n);
    setFlag(HF, h);
    setFlag(CF, c);
}

auto CPU::getBC() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(B) << 8) | C);
}

auto CPU::getDE() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(D) << 8) | E);
}

auto CPU::getHL() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(H) << 8) | L);
}

auto CPU::setBC(uint16_t value) -> void {
    B = static_cast<uint8_t>(value >> 8);
    C = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::setDE(uint16_t value) -> void {
    D = static_cast<uint8_t>(value >> 8);
    E = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::setHL(uint16_t value) -> void {
    H = static_cast<uint8_t>(value >> 8);
    L = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::readR8(uint8_t code) -> uint8_t {
    switch (code & 0x07u) {
    case 0: return B;
    case 1: return C;
    case 2: return D;
    case 3: return E;
    case 4: return H;
    case 5: return L;
    case 6: return bus.read(getHL());
    default: return A;
    }
}

auto CPU::writeR8(uint8_t code, uint8_t value) -> void {
    switch (code & 0x07u) {
    case 0: B = value; break;
    case 1: C = value; break;
    case 2: D = value; break;
    case 3: E = value; break;
    case 4: H = value; break;
    case 5: L = value; break;
    case 6: bus.write(getHL(), value); break;
    default: A = value; break;
    }
}

auto CPU::push16(uint16_t value) -> void {
    --SP;
    bus.write(SP, static_cast<uint8_t>(value >> 8));
    --SP;
    bus.write(SP, static_cast<uint8_t>(value & 0x00FFu));
}

auto CPU::pop16() -> uint16_t {
    const uint8_t low = bus.read(SP++);
    const uint8_t high = bus.read(SP++);
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

auto CPU::inc8(uint8_t value) -> uint8_t {
    const uint8_t result = static_cast<uint8_t>(value + 1u);
    setFlag(Z, result == 0);
    setFlag(N, false);
    setFlag(HF, (value & 0x0Fu) == 0x0Fu);
    return result;
}

auto CPU::dec8(uint8_t value) -> uint8_t {
    const uint8_t result = static_cast<uint8_t>(value - 1u);
    setFlag(Z, result == 0);
    setFlag(N, true);
    setFlag(HF, (value & 0x0Fu) == 0x00u);
    return result;
}

auto CPU::addA(uint8_t value, bool withCarry) -> void {
    const uint8_t carry = (withCarry && getFlag(CF)) ? 1u : 0u;
    const uint16_t sum = static_cast<uint16_t>(A) + value + carry;
    const uint16_t halfSum = static_cast<uint16_t>(A & 0x0Fu)
        + static_cast<uint16_t>(value & 0x0Fu)
        + static_cast<uint16_t>(carry);
    const bool half = halfSum > static_cast<uint16_t>(0x000Fu);
    A = static_cast<uint8_t>(sum);
    setZNHC(A == 0, false, half, sum > 0x00FFu);
}

auto CPU::subA(uint8_t value, bool withCarry) -> void {
    const uint8_t carry = (withCarry && getFlag(CF)) ? 1u : 0u;
    const uint16_t rhs = static_cast<uint16_t>(value) + carry;
    const bool half = static_cast<uint16_t>(A & 0x0Fu)
        < static_cast<uint16_t>((value & 0x0Fu) + carry);
    const bool full = static_cast<uint16_t>(A) < rhs;
    A = static_cast<uint8_t>(static_cast<uint16_t>(A) - rhs);
    setZNHC(A == 0, true, half, full);
}

auto CPU::cpA(uint8_t value) -> void {
    const uint8_t result = static_cast<uint8_t>(A - value);
    setZNHC(result == 0, true, (A & 0x0Fu) < (value & 0x0Fu), A < value);
}

auto CPU::andA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A & value);
    setZNHC(A == 0, false, true, false);
}

auto CPU::xorA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A ^ value);
    setZNHC(A == 0, false, false, false);
}

auto CPU::orA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A | value);
    setZNHC(A == 0, false, false, false);
}

auto CPU::addHL(uint16_t value) -> void {
    const uint16_t old = getHL();
    const uint32_t sum = static_cast<uint32_t>(old) + value;
    setFlag(N, false);
    setFlag(HF, static_cast<uint32_t>(old & 0x0FFFu)
        + static_cast<uint32_t>(value & 0x0FFFu) > 0x0FFFu);
    setFlag(CF, sum > 0xFFFFu);
    setHL(static_cast<uint16_t>(sum));
}

auto CPU::addSignedToSP(int8_t offset) -> uint16_t {
    const uint16_t old = SP;
    const uint8_t u = static_cast<uint8_t>(offset);
    const uint16_t result = static_cast<uint16_t>(old + static_cast<int16_t>(offset));
    setZNHC(false, false,
        static_cast<uint16_t>(old & 0x000Fu) + (u & 0x0Fu) > 0x000Fu,
        static_cast<uint16_t>(static_cast<uint16_t>(old & 0x00FFu)
            + static_cast<uint16_t>(u)) > static_cast<uint16_t>(0x00FFu));
    return result;
}

auto CPU::relativeJump(bool condition) -> int {
    const int8_t offset = static_cast<int8_t>(fetch8());
    if (condition) {
        PC = static_cast<uint16_t>(PC + static_cast<int16_t>(offset));
        return 12;
    }
    return 8;
}

auto CPU::conditionalJP(bool condition) -> int {
    const uint16_t address = fetch16();
    if (condition) {
        PC = address;
        return 16;
    }
    return 12;
}

auto CPU::conditionalCall(bool condition) -> int {
    const uint16_t address = fetch16();
    if (condition) {
        push16(PC);
        PC = address;
        return 24;
    }
    return 12;
}

auto CPU::conditionalRet(bool condition) -> int {
    if (condition) {
        PC = pop16();
        return 20;
    }
    return 8;
}

auto CPU::restart(uint16_t vector) -> int {
    push16(PC);
    PC = vector;
    return 16;
}

auto CPU::executeCB(uint8_t cb) -> int {
    const uint8_t reg = static_cast<uint8_t>(cb & 0x07u);
    const bool memory = reg == 6;
    uint8_t value = readR8(reg);

    if (cb < 0x40u) {
        const uint8_t operation = static_cast<uint8_t>((cb >> 3) & 0x07u);
        uint8_t result = value;
        bool carry = false;

        switch (operation) {
        case 0: // RLC
            carry = (value & 0x80u) != 0;
            result = static_cast<uint8_t>((value << 1) | (carry ? 1u : 0u));
            break;
        case 1: // RRC
            carry = (value & 0x01u) != 0;
            result = static_cast<uint8_t>((value >> 1) | (carry ? 0x80u : 0u));
            break;
        case 2: { // RL
            const bool oldCarry = getFlag(CF);
            carry = (value & 0x80u) != 0;
            result = static_cast<uint8_t>((value << 1) | (oldCarry ? 1u : 0u));
            break;
        }
        case 3: { // RR
            const bool oldCarry = getFlag(CF);
            carry = (value & 0x01u) != 0;
            result = static_cast<uint8_t>((value >> 1) | (oldCarry ? 0x80u : 0u));
            break;
        }
        case 4: // SLA
            carry = (value & 0x80u) != 0;
            result = static_cast<uint8_t>(value << 1);
            break;
        case 5: // SRA
            carry = (value & 0x01u) != 0;
            result = static_cast<uint8_t>((value >> 1) | (value & 0x80u));
            break;
        case 6: // SWAP
            result = static_cast<uint8_t>((value << 4) | (value >> 4));
            carry = false;
            break;
        default: // SRL
            carry = (value & 0x01u) != 0;
            result = static_cast<uint8_t>(value >> 1);
            break;
        }

        writeR8(reg, result);
        setZNHC(result == 0, false, false, carry);
        return memory ? 16 : 8;
    }

    const uint8_t bit = static_cast<uint8_t>((cb >> 3) & 0x07u);
    const uint8_t mask = static_cast<uint8_t>(1u << bit);

    if (cb < 0x80u) { // BIT b,r
        setFlag(Z, (value & mask) == 0);
        setFlag(N, false);
        setFlag(HF, true);
        return memory ? 12 : 8;
    }

    if (cb < 0xC0u) { // RES b,r
        value = static_cast<uint8_t>(value & static_cast<uint8_t>(~mask));
        writeR8(reg, value);
        return memory ? 16 : 8;
    }

    // SET b,r
    value = static_cast<uint8_t>(value | mask);
    writeR8(reg, value);
    return memory ? 16 : 8;
}

auto CPU::execute(uint8_t opcode) -> int {
    // 0x40..0x7F: LD r8,r8 (0x76 is HALT)
    if (opcode >= 0x40 && opcode <= 0x7F) {
        if (opcode == 0x76) {
            halted = true;
            return 4;
        }
        const uint8_t dst = static_cast<uint8_t>((opcode >> 3) & 0x07);
        const uint8_t src = static_cast<uint8_t>(opcode & 0x07);
        writeR8(dst, readR8(src));
        return (dst == 6 || src == 6) ? 8 : 4;
    }

    // 0x80..0xBF: ALU A,r8
    if (opcode >= 0x80 && opcode <= 0xBF) {
        const uint8_t src = static_cast<uint8_t>(opcode & 0x07);
        const uint8_t value = readR8(src);
        switch ((opcode >> 3) & 0x07) {
        case 0: addA(value, false); break; // ADD
        case 1: addA(value, true);  break; // ADC
        case 2: subA(value, false); break; // SUB
        case 3: subA(value, true);  break; // SBC
        case 4: andA(value);        break;
        case 5: xorA(value);        break;
        case 6: orA(value);         break;
        case 7: cpA(value);         break;
        }
        return src == 6 ? 8 : 4;
    }

    switch (opcode) {
    case 0x00: // NOP
        return 4;

    case 0x01: setBC(fetch16()); return 12;
    case 0x02: bus.write(getBC(), A); return 8;
    case 0x03: setBC(static_cast<uint16_t>(getBC() + 1)); return 8;
    case 0x04: B = inc8(B); return 4;
    case 0x05: B = dec8(B); return 4;
    case 0x06: B = fetch8(); return 8;
    case 0x07: { // RLCA
        const bool carry = (A & 0x80) != 0;
        A = static_cast<uint8_t>((A << 1) | (carry ? 1 : 0));
        setZNHC(false, false, false, carry);
        return 4;
    }
    case 0x08: { // LD (a16),SP
        const uint16_t address = fetch16();
        bus.write(address, static_cast<uint8_t>(SP));
        bus.write(static_cast<uint16_t>(address + 1), static_cast<uint8_t>(SP >> 8));
        return 20;
    }
    case 0x09: addHL(getBC()); return 8;
    case 0x0A: A = bus.read(getBC()); return 8;
    case 0x0B: setBC(static_cast<uint16_t>(getBC() - 1)); return 8;
    case 0x0C: C = inc8(C); return 4;
    case 0x0D: C = dec8(C); return 4;
    case 0x0E: C = fetch8(); return 8;
    case 0x0F: { // RRCA
        const bool carry = (A & 0x01) != 0;
        A = static_cast<uint8_t>((A >> 1) | (carry ? 0x80 : 0));
        setZNHC(false, false, false, carry);
        return 4;
    }

    case 0x10: // STOP 0
        (void)fetch8(); // The second byte is normally 0x00.
        halted = true;  // Simplified until joypad/speed-switch handling exists.
        return 4;
    case 0x11: setDE(fetch16()); return 12;
    case 0x12: bus.write(getDE(), A); return 8;
    case 0x13: setDE(static_cast<uint16_t>(getDE() + 1)); return 8;
    case 0x14: D = inc8(D); return 4;
    case 0x15: D = dec8(D); return 4;
    case 0x16: D = fetch8(); return 8;
    case 0x17: { // RLA
        const bool oldCarry = getFlag(CF);
        const bool carry = (A & 0x80) != 0;
        A = static_cast<uint8_t>((A << 1) | (oldCarry ? 1 : 0));
        setZNHC(false, false, false, carry);
        return 4;
    }
    case 0x18: return relativeJump(true);
    case 0x19: addHL(getDE()); return 8;
    case 0x1A: A = bus.read(getDE()); return 8;
    case 0x1B: setDE(static_cast<uint16_t>(getDE() - 1)); return 8;
    case 0x1C: E = inc8(E); return 4;
    case 0x1D: E = dec8(E); return 4;
    case 0x1E: E = fetch8(); return 8;
    case 0x1F: { // RRA
        const bool oldCarry = getFlag(CF);
        const bool carry = (A & 0x01) != 0;
        A = static_cast<uint8_t>((A >> 1) | (oldCarry ? 0x80 : 0));
        setZNHC(false, false, false, carry);
        return 4;
    }

    case 0x20: return relativeJump(!getFlag(Z));
    case 0x21: setHL(fetch16()); return 12;
    case 0x22: { const uint16_t a = getHL(); bus.write(a, A); setHL(static_cast<uint16_t>(a + 1)); return 8; }
    case 0x23: setHL(static_cast<uint16_t>(getHL() + 1)); return 8;
    case 0x24: H = inc8(H); return 4;
    case 0x25: H = dec8(H); return 4;
    case 0x26: H = fetch8(); return 8;
    case 0x27: { // DAA
        uint8_t correction = 0;
        bool carry = getFlag(CF);
        if (!getFlag(N)) {
            if (getFlag(HF) || (A & 0x0F) > 9) {
                correction |= 0x06;
            }
            if (carry || A > 0x99) {
                correction |= 0x60;
                carry = true;
            }
            A = static_cast<uint8_t>(A + correction);
        }
        else {
            if (getFlag(HF)) correction |= 0x06;
            if (carry) correction |= 0x60;
            A = static_cast<uint8_t>(A - correction);
        }
        setFlag(Z, A == 0);
        setFlag(HF, false);
        setFlag(CF, carry);
        return 4;
    }
    case 0x28: return relativeJump(getFlag(Z));
    case 0x29: addHL(getHL()); return 8;
    case 0x2A: { const uint16_t a = getHL(); A = bus.read(a); setHL(static_cast<uint16_t>(a + 1)); return 8; }
    case 0x2B: setHL(static_cast<uint16_t>(getHL() - 1)); return 8;
    case 0x2C: L = inc8(L); return 4;
    case 0x2D: L = dec8(L); return 4;
    case 0x2E: L = fetch8(); return 8;
    case 0x2F: // CPL
        A = static_cast<uint8_t>(~A);
        setFlag(N, true);
        setFlag(HF, true);
        return 4;

    case 0x30: return relativeJump(!getFlag(CF));
    case 0x31: SP = fetch16(); return 12;
    case 0x32: { const uint16_t a = getHL(); bus.write(a, A); setHL(static_cast<uint16_t>(a - 1)); return 8; }
    case 0x33: ++SP; return 8;
    case 0x34: { const uint16_t a = getHL(); bus.write(a, inc8(bus.read(a))); return 12; }
    case 0x35: { const uint16_t a = getHL(); bus.write(a, dec8(bus.read(a))); return 12; }
    case 0x36: bus.write(getHL(), fetch8()); return 12;
    case 0x37: // SCF
        setFlag(N, false);
        setFlag(HF, false);
        setFlag(CF, true);
        return 4;
    case 0x38: return relativeJump(getFlag(CF));
    case 0x39: addHL(SP); return 8;
    case 0x3A: { const uint16_t a = getHL(); A = bus.read(a); setHL(static_cast<uint16_t>(a - 1)); return 8; }
    case 0x3B: --SP; return 8;
    case 0x3C: A = inc8(A); return 4;
    case 0x3D: A = dec8(A); return 4;
    case 0x3E: A = fetch8(); return 8;
    case 0x3F: { // CCF
        const bool carry = !getFlag(CF);
        setFlag(N, false);
        setFlag(HF, false);
        setFlag(CF, carry);
        return 4;
    }

    case 0xC0: return conditionalRet(!getFlag(Z));
    case 0xC1: setBC(pop16()); return 12;
    case 0xC2: return conditionalJP(!getFlag(Z));
    case 0xC3: PC = fetch16(); return 16;
    case 0xC4: return conditionalCall(!getFlag(Z));
    case 0xC5: push16(getBC()); return 16;
    case 0xC6: addA(fetch8(), false); return 8;
    case 0xC7: return restart(0x00);
    case 0xC8: return conditionalRet(getFlag(Z));
    case 0xC9: PC = pop16(); return 16;
    case 0xCA: return conditionalJP(getFlag(Z));
    case 0xCB: return executeCB(fetch8());
    case 0xCC: return conditionalCall(getFlag(Z));
    case 0xCD: { const uint16_t address = fetch16(); push16(PC); PC = address; return 24; }
    case 0xCE: addA(fetch8(), true); return 8;
    case 0xCF: return restart(0x08);

    case 0xD0: return conditionalRet(!getFlag(CF));
    case 0xD1: setDE(pop16()); return 12;
    case 0xD2: return conditionalJP(!getFlag(CF));
    case 0xD4: return conditionalCall(!getFlag(CF));
    case 0xD5: push16(getDE()); return 16;
    case 0xD6: subA(fetch8(), false); return 8;
    case 0xD7: return restart(0x10);
    case 0xD8: return conditionalRet(getFlag(CF));
    case 0xD9: // RETI
        PC = pop16();
        ime = true;
        imeEnableDelay = 0;
        return 16;
    case 0xDA: return conditionalJP(getFlag(CF));
    case 0xDC: return conditionalCall(getFlag(CF));
    case 0xDE: subA(fetch8(), true); return 8;
    case 0xDF: return restart(0x18);

    case 0xE0: bus.write(static_cast<uint16_t>(0xFF00u + fetch8()), A); return 12;
    case 0xE1: setHL(pop16()); return 12;
    case 0xE2: bus.write(static_cast<uint16_t>(0xFF00u + C), A); return 8;
    case 0xE5: push16(getHL()); return 16;
    case 0xE6: andA(fetch8()); return 8;
    case 0xE7: return restart(0x20);
    case 0xE8: { const int8_t e = static_cast<int8_t>(fetch8()); SP = addSignedToSP(e); return 16; }
    case 0xE9: PC = getHL(); return 4;
    case 0xEA: { const uint16_t address = fetch16(); bus.write(address, A); return 16; }
    case 0xEE: xorA(fetch8()); return 8;
    case 0xEF: return restart(0x28);

    case 0xF0: A = bus.read(static_cast<uint16_t>(0xFF00u + fetch8())); return 12;
    case 0xF1: {
        const uint16_t value = pop16();
        A = static_cast<uint8_t>(value >> 8);
        F = static_cast<uint8_t>(value & 0xF0); // Low nibble of F is always zero.
        return 12;
    }
    case 0xF2: A = bus.read(static_cast<uint16_t>(0xFF00u + C)); return 8;
    case 0xF3: ime = false; imeEnableDelay = 0; return 4; // DI
    case 0xF5: push16(static_cast<uint16_t>((static_cast<uint16_t>(A) << 8) | (F & 0xF0))); return 16;
    case 0xF6: orA(fetch8()); return 8;
    case 0xF7: return restart(0x30);
    case 0xF8: { const int8_t e = static_cast<int8_t>(fetch8()); setHL(addSignedToSP(e)); return 12; }
    case 0xF9: SP = getHL(); return 8;
    case 0xFA: A = bus.read(fetch16()); return 16;
    case 0xFB: if (!ime && imeEnableDelay == 0) { imeEnableDelay = 2; } return 4;
    case 0xFE: cpA(fetch8()); return 8;
    case 0xFF: return restart(0x38);

    // Undefined LR35902 opcodes: D3 DB DD E3 E4 EB EC ED F4 FC FD.
    default:
        throw std::runtime_error("Undefined opcode");
        return 4;
    }
}

auto CPU::pendingInterrupts() -> uint8_t {
    constexpr uint16_t IE = 0xFFFF;
    constexpr uint16_t IF = 0xFF0F;

    return static_cast<uint8_t>(bus.read(IE) & bus.read(IF) & 0x1Fu);
}

auto CPU::serviceInterrupt(uint8_t pending) -> int {
    static constexpr uint16_t vectors[] = {
        0x0040, // VBlank
        0x0048, // LCD STAT
        0x0050, // Timer
        0x0058, // Serial
        0x0060  // Joypad
    };

    constexpr uint16_t IF = 0xFF0F;

    // IME is automatically disabled on interrupt entry
    ime = false;
    imeEnableDelay = 0;

    // The interrupt ends the HALT state
    halted = false;

    for (uint8_t bit = 0; bit < 5; ++bit) {
        const uint8_t mask =
            static_cast<uint8_t>(1u << bit);

        if ((pending & mask) == 0) {
            continue;
        }

        // --------------------------------------------
        // Clear the accepted IF bit
        // --------------------------------------------

        uint8_t interruptFlags =
            bus.read(IF);

        interruptFlags =
            static_cast<uint8_t>(
                interruptFlags
                & static_cast<uint8_t>(~mask)
                );

        bus.write(IF, interruptFlags);

        // --------------------------------------------
        // Equivalent to CALL interrupt vector
        // --------------------------------------------

        push16(PC);

        PC = vectors[bit];

        // 5 M-cycles = 20 clock cycles
        return 20;
    }

    return 0;
}

auto CPU::updateIME() -> void {
    if (imeEnableDelay == 0) {
        return;
    }

    --imeEnableDelay;

    if (imeEnableDelay == 0) {
        ime = true;
    }
}