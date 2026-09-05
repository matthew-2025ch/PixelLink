#include "CPU.hpp"

namespace PixelLink::GameBoy {

CPU::CPU(Bus& bus) : bus(bus) {
    Reset();
}

auto CPU::Reset() -> void {
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

auto CPU::Step() -> int {
    const auto finishStep = [this](int cycles) {
        bus.Tick(static_cast<uint32_t>(cycles));
        return cycles;
        };

    const uint8_t pending =
        PendingInterrupts();

    if (pending != 0) {
        halted = false;

        if (ime) {
            return finishStep(
                ServiceInterrupt(pending)
            );
        }
    }

    if (halted) {
        return finishStep(4);
    }

    const int cycles =
        Execute(Fetch8());

    UpdateIME();

    return finishStep(cycles);
}

auto CPU::Fetch8() -> uint8_t {
    return bus.Read(PC++);
}

auto CPU::Fetch16() -> uint16_t {
    const uint8_t low = Fetch8();
    const uint8_t high = Fetch8();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

auto CPU::GetFlag(Flag flag) const -> bool {
    return (F & static_cast<uint8_t>(flag)) != 0;
}

auto CPU::SetFlag(Flag flag, bool value) -> void {
    const uint8_t mask = static_cast<uint8_t>(flag);
    if (value) {
        F = static_cast<uint8_t>(F | mask);
    } else {
        F = static_cast<uint8_t>(F & static_cast<uint8_t>(~mask));
    }
    F &= 0xF0;
}

auto CPU::SetZNHC(bool z, bool n, bool h, bool c) -> void {
    SetFlag(Z, z);
    SetFlag(N, n);
    SetFlag(HF, h);
    SetFlag(CF, c);
}

auto CPU::GetBC() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(B) << 8) | C);
}

auto CPU::GetDE() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(D) << 8) | E);
}

auto CPU::GetHL() const -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(H) << 8) | L);
}

auto CPU::SetBC(uint16_t value) -> void {
    B = static_cast<uint8_t>(value >> 8);
    C = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::SetDE(uint16_t value) -> void {
    D = static_cast<uint8_t>(value >> 8);
    E = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::SetHL(uint16_t value) -> void {
    H = static_cast<uint8_t>(value >> 8);
    L = static_cast<uint8_t>(value & 0x00FFu);
}

auto CPU::ReadR8(uint8_t code) -> uint8_t {
    switch (code & 0x07u) {
    case 0: return B;
    case 1: return C;
    case 2: return D;
    case 3: return E;
    case 4: return H;
    case 5: return L;
    case 6: return bus.Read(GetHL());
    default: return A;
    }
}

auto CPU::WriteR8(uint8_t code, uint8_t value) -> void {
    switch (code & 0x07u) {
    case 0: B = value; break;
    case 1: C = value; break;
    case 2: D = value; break;
    case 3: E = value; break;
    case 4: H = value; break;
    case 5: L = value; break;
    case 6: bus.Write(GetHL(), value); break;
    default: A = value; break;
    }
}

auto CPU::Push16(uint16_t value) -> void {
    --SP;
    bus.Write(SP, static_cast<uint8_t>(value >> 8));
    --SP;
    bus.Write(SP, static_cast<uint8_t>(value & 0x00FFu));
}

auto CPU::Pop16() -> uint16_t {
    const uint8_t low = bus.Read(SP++);
    const uint8_t high = bus.Read(SP++);
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

auto CPU::Inc8(uint8_t value) -> uint8_t {
    const uint8_t result = static_cast<uint8_t>(value + 1u);
    SetFlag(Z, result == 0);
    SetFlag(N, false);
    SetFlag(HF, (value & 0x0Fu) == 0x0Fu);
    return result;
}

auto CPU::Dec8(uint8_t value) -> uint8_t {
    const uint8_t result = static_cast<uint8_t>(value - 1u);
    SetFlag(Z, result == 0);
    SetFlag(N, true);
    SetFlag(HF, (value & 0x0Fu) == 0x00u);
    return result;
}

auto CPU::AddA(uint8_t value, bool withCarry) -> void {
    const uint8_t carry = (withCarry && GetFlag(CF)) ? 1u : 0u;
    const uint16_t sum = static_cast<uint16_t>(A) + value + carry;
    const uint16_t halfSum = static_cast<uint16_t>(A & 0x0Fu)
        + static_cast<uint16_t>(value & 0x0Fu)
        + static_cast<uint16_t>(carry);
    const bool half = halfSum > static_cast<uint16_t>(0x000Fu);
    A = static_cast<uint8_t>(sum);
    SetZNHC(A == 0, false, half, sum > 0x00FFu);
}

auto CPU::SubA(uint8_t value, bool withCarry) -> void {
    const uint8_t carry = (withCarry && GetFlag(CF)) ? 1u : 0u;
    const uint16_t rhs = static_cast<uint16_t>(value) + carry;
    const bool half = static_cast<uint16_t>(A & 0x0Fu)
        < static_cast<uint16_t>((value & 0x0Fu) + carry);
    const bool full = static_cast<uint16_t>(A) < rhs;
    A = static_cast<uint8_t>(static_cast<uint16_t>(A) - rhs);
    SetZNHC(A == 0, true, half, full);
}

auto CPU::CpA(uint8_t value) -> void {
    const uint8_t result = static_cast<uint8_t>(A - value);
    SetZNHC(result == 0, true, (A & 0x0Fu) < (value & 0x0Fu), A < value);
}

auto CPU::AndA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A & value);
    SetZNHC(A == 0, false, true, false);
}

auto CPU::XorA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A ^ value);
    SetZNHC(A == 0, false, false, false);
}

auto CPU::OrA(uint8_t value) -> void {
    A = static_cast<uint8_t>(A | value);
    SetZNHC(A == 0, false, false, false);
}

auto CPU::AddHL(uint16_t value) -> void {
    const uint16_t old = GetHL();
    const uint32_t sum = static_cast<uint32_t>(old) + value;
    SetFlag(N, false);
    SetFlag(HF, static_cast<uint32_t>(old & 0x0FFFu)
        + static_cast<uint32_t>(value & 0x0FFFu) > 0x0FFFu);
    SetFlag(CF, sum > 0xFFFFu);
    SetHL(static_cast<uint16_t>(sum));
}

auto CPU::AddSignedToSP(int8_t offset) -> uint16_t {
    const uint16_t old = SP;
    const uint8_t u = static_cast<uint8_t>(offset);
    const uint16_t result = static_cast<uint16_t>(old + static_cast<int16_t>(offset));
    SetZNHC(false, false,
        static_cast<uint16_t>(old & 0x000Fu) + (u & 0x0Fu) > 0x000Fu,
        static_cast<uint16_t>(static_cast<uint16_t>(old & 0x00FFu)
            + static_cast<uint16_t>(u)) > static_cast<uint16_t>(0x00FFu));
    return result;
}

auto CPU::RelativeJump(bool condition) -> int {
    const int8_t offset = static_cast<int8_t>(Fetch8());
    if (condition) {
        PC = static_cast<uint16_t>(PC + static_cast<int16_t>(offset));
        return 12;
    }
    return 8;
}

auto CPU::ConditionalJP(bool condition) -> int {
    const uint16_t address = Fetch16();
    if (condition) {
        PC = address;
        return 16;
    }
    return 12;
}

auto CPU::ConditionalCall(bool condition) -> int {
    const uint16_t address = Fetch16();
    if (condition) {
        Push16(PC);
        PC = address;
        return 24;
    }
    return 12;
}

auto CPU::ConditionalRet(bool condition) -> int {
    if (condition) {
        PC = Pop16();
        return 20;
    }
    return 8;
}

auto CPU::Restart(uint16_t vector) -> int {
    Push16(PC);
    PC = vector;
    return 16;
}

auto CPU::ExecuteCB(uint8_t cb) -> int {
    const uint8_t reg = static_cast<uint8_t>(cb & 0x07u);
    const bool memory = reg == 6;
    uint8_t value = ReadR8(reg);

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
            const bool oldCarry = GetFlag(CF);
            carry = (value & 0x80u) != 0;
            result = static_cast<uint8_t>((value << 1) | (oldCarry ? 1u : 0u));
            break;
        }
        case 3: { // RR
            const bool oldCarry = GetFlag(CF);
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

        WriteR8(reg, result);
        SetZNHC(result == 0, false, false, carry);
        return memory ? 16 : 8;
    }

    const uint8_t bit = static_cast<uint8_t>((cb >> 3) & 0x07u);
    const uint8_t mask = static_cast<uint8_t>(1u << bit);

    if (cb < 0x80u) { // BIT b,r
        SetFlag(Z, (value & mask) == 0);
        SetFlag(N, false);
        SetFlag(HF, true);
        return memory ? 12 : 8;
    }

    if (cb < 0xC0u) { // RES b,r
        value = static_cast<uint8_t>(value & static_cast<uint8_t>(~mask));
        WriteR8(reg, value);
        return memory ? 16 : 8;
    }

    // SET b,r
    value = static_cast<uint8_t>(value | mask);
    WriteR8(reg, value);
    return memory ? 16 : 8;
}

auto CPU::Execute(uint8_t opcode) -> int {
    // 0x40..0x7F: LD r8,r8 (0x76 is HALT)
    if (opcode >= 0x40 && opcode <= 0x7F) {
        if (opcode == 0x76) {
            halted = true;
            return 4;
        }
        const uint8_t dst = static_cast<uint8_t>((opcode >> 3) & 0x07);
        const uint8_t src = static_cast<uint8_t>(opcode & 0x07);
        WriteR8(dst, ReadR8(src));
        return (dst == 6 || src == 6) ? 8 : 4;
    }

    // 0x80..0xBF: ALU A,r8
    if (opcode >= 0x80 && opcode <= 0xBF) {
        const uint8_t src = static_cast<uint8_t>(opcode & 0x07);
        const uint8_t value = ReadR8(src);
        switch ((opcode >> 3) & 0x07) {
        case 0: AddA(value, false); break; // ADD
        case 1: AddA(value, true);  break; // ADC
        case 2: SubA(value, false); break; // SUB
        case 3: SubA(value, true);  break; // SBC
        case 4: AndA(value);        break;
        case 5: XorA(value);        break;
        case 6: OrA(value);         break;
        case 7: CpA(value);         break;
        }
        return src == 6 ? 8 : 4;
    }

    switch (opcode) {
    case 0x00: // NOP
        return 4;

    case 0x01: SetBC(Fetch16()); return 12;
    case 0x02: bus.Write(GetBC(), A); return 8;
    case 0x03: SetBC(static_cast<uint16_t>(GetBC() + 1)); return 8;
    case 0x04: B = Inc8(B); return 4;
    case 0x05: B = Dec8(B); return 4;
    case 0x06: B = Fetch8(); return 8;
    case 0x07: { // RLCA
        const bool carry = (A & 0x80) != 0;
        A = static_cast<uint8_t>((A << 1) | (carry ? 1 : 0));
        SetZNHC(false, false, false, carry);
        return 4;
    }
    case 0x08: { // LD (a16),SP
        const uint16_t address = Fetch16();
        bus.Write(address, static_cast<uint8_t>(SP));
        bus.Write(static_cast<uint16_t>(address + 1), static_cast<uint8_t>(SP >> 8));
        return 20;
    }
    case 0x09: AddHL(GetBC()); return 8;
    case 0x0A: A = bus.Read(GetBC()); return 8;
    case 0x0B: SetBC(static_cast<uint16_t>(GetBC() - 1)); return 8;
    case 0x0C: C = Inc8(C); return 4;
    case 0x0D: C = Dec8(C); return 4;
    case 0x0E: C = Fetch8(); return 8;
    case 0x0F: { // RRCA
        const bool carry = (A & 0x01) != 0;
        A = static_cast<uint8_t>((A >> 1) | (carry ? 0x80 : 0));
        SetZNHC(false, false, false, carry);
        return 4;
    }

    case 0x10: // STOP 0
        (void)Fetch8(); // The second byte is normally 0x00.
        halted = true;  // Simplified until joypad/speed-switch handling exists.
        return 4;
    case 0x11: SetDE(Fetch16()); return 12;
    case 0x12: bus.Write(GetDE(), A); return 8;
    case 0x13: SetDE(static_cast<uint16_t>(GetDE() + 1)); return 8;
    case 0x14: D = Inc8(D); return 4;
    case 0x15: D = Dec8(D); return 4;
    case 0x16: D = Fetch8(); return 8;
    case 0x17: { // RLA
        const bool oldCarry = GetFlag(CF);
        const bool carry = (A & 0x80) != 0;
        A = static_cast<uint8_t>((A << 1) | (oldCarry ? 1 : 0));
        SetZNHC(false, false, false, carry);
        return 4;
    }
    case 0x18: return RelativeJump(true);
    case 0x19: AddHL(GetDE()); return 8;
    case 0x1A: A = bus.Read(GetDE()); return 8;
    case 0x1B: SetDE(static_cast<uint16_t>(GetDE() - 1)); return 8;
    case 0x1C: E = Inc8(E); return 4;
    case 0x1D: E = Dec8(E); return 4;
    case 0x1E: E = Fetch8(); return 8;
    case 0x1F: { // RRA
        const bool oldCarry = GetFlag(CF);
        const bool carry = (A & 0x01) != 0;
        A = static_cast<uint8_t>((A >> 1) | (oldCarry ? 0x80 : 0));
        SetZNHC(false, false, false, carry);
        return 4;
    }

    case 0x20: return RelativeJump(!GetFlag(Z));
    case 0x21: SetHL(Fetch16()); return 12;
    case 0x22: { const uint16_t a = GetHL(); bus.Write(a, A); SetHL(static_cast<uint16_t>(a + 1)); return 8; }
    case 0x23: SetHL(static_cast<uint16_t>(GetHL() + 1)); return 8;
    case 0x24: H = Inc8(H); return 4;
    case 0x25: H = Dec8(H); return 4;
    case 0x26: H = Fetch8(); return 8;
    case 0x27: { // DAA
        uint8_t correction = 0;
        bool carry = GetFlag(CF);
        if (!GetFlag(N)) {
            if (GetFlag(HF) || (A & 0x0F) > 9) {
                correction |= 0x06;
            }
            if (carry || A > 0x99) {
                correction |= 0x60;
                carry = true;
            }
            A = static_cast<uint8_t>(A + correction);
        }
        else {
            if (GetFlag(HF)) correction |= 0x06;
            if (carry) correction |= 0x60;
            A = static_cast<uint8_t>(A - correction);
        }
        SetFlag(Z, A == 0);
        SetFlag(HF, false);
        SetFlag(CF, carry);
        return 4;
    }
    case 0x28: return RelativeJump(GetFlag(Z));
    case 0x29: AddHL(GetHL()); return 8;
    case 0x2A: { const uint16_t a = GetHL(); A = bus.Read(a); SetHL(static_cast<uint16_t>(a + 1)); return 8; }
    case 0x2B: SetHL(static_cast<uint16_t>(GetHL() - 1)); return 8;
    case 0x2C: L = Inc8(L); return 4;
    case 0x2D: L = Dec8(L); return 4;
    case 0x2E: L = Fetch8(); return 8;
    case 0x2F: // CPL
        A = static_cast<uint8_t>(~A);
        SetFlag(N, true);
        SetFlag(HF, true);
        return 4;

    case 0x30: return RelativeJump(!GetFlag(CF));
    case 0x31: SP = Fetch16(); return 12;
    case 0x32: { const uint16_t a = GetHL(); bus.Write(a, A); SetHL(static_cast<uint16_t>(a - 1)); return 8; }
    case 0x33: ++SP; return 8;
    case 0x34: { const uint16_t a = GetHL(); bus.Write(a, Inc8(bus.Read(a))); return 12; }
    case 0x35: { const uint16_t a = GetHL(); bus.Write(a, Dec8(bus.Read(a))); return 12; }
    case 0x36: bus.Write(GetHL(), Fetch8()); return 12;
    case 0x37: // SCF
        SetFlag(N, false);
        SetFlag(HF, false);
        SetFlag(CF, true);
        return 4;
    case 0x38: return RelativeJump(GetFlag(CF));
    case 0x39: AddHL(SP); return 8;
    case 0x3A: { const uint16_t a = GetHL(); A = bus.Read(a); SetHL(static_cast<uint16_t>(a - 1)); return 8; }
    case 0x3B: --SP; return 8;
    case 0x3C: A = Inc8(A); return 4;
    case 0x3D: A = Dec8(A); return 4;
    case 0x3E: A = Fetch8(); return 8;
    case 0x3F: { // CCF
        const bool carry = !GetFlag(CF);
        SetFlag(N, false);
        SetFlag(HF, false);
        SetFlag(CF, carry);
        return 4;
    }

    case 0xC0: return ConditionalRet(!GetFlag(Z));
    case 0xC1: SetBC(Pop16()); return 12;
    case 0xC2: return ConditionalJP(!GetFlag(Z));
    case 0xC3: PC = Fetch16(); return 16;
    case 0xC4: return ConditionalCall(!GetFlag(Z));
    case 0xC5: Push16(GetBC()); return 16;
    case 0xC6: AddA(Fetch8(), false); return 8;
    case 0xC7: return Restart(0x00);
    case 0xC8: return ConditionalRet(GetFlag(Z));
    case 0xC9: PC = Pop16(); return 16;
    case 0xCA: return ConditionalJP(GetFlag(Z));
    case 0xCB: return ExecuteCB(Fetch8());
    case 0xCC: return ConditionalCall(GetFlag(Z));
    case 0xCD: { const uint16_t address = Fetch16(); Push16(PC); PC = address; return 24; }
    case 0xCE: AddA(Fetch8(), true); return 8;
    case 0xCF: return Restart(0x08);

    case 0xD0: return ConditionalRet(!GetFlag(CF));
    case 0xD1: SetDE(Pop16()); return 12;
    case 0xD2: return ConditionalJP(!GetFlag(CF));
    case 0xD4: return ConditionalCall(!GetFlag(CF));
    case 0xD5: Push16(GetDE()); return 16;
    case 0xD6: SubA(Fetch8(), false); return 8;
    case 0xD7: return Restart(0x10);
    case 0xD8: return ConditionalRet(GetFlag(CF));
    case 0xD9: // RETI
        PC = Pop16();
        ime = true;
        imeEnableDelay = 0;
        return 16;
    case 0xDA: return ConditionalJP(GetFlag(CF));
    case 0xDC: return ConditionalCall(GetFlag(CF));
    case 0xDE: SubA(Fetch8(), true); return 8;
    case 0xDF: return Restart(0x18);

    case 0xE0: bus.Write(static_cast<uint16_t>(0xFF00u + Fetch8()), A); return 12;
    case 0xE1: SetHL(Pop16()); return 12;
    case 0xE2: bus.Write(static_cast<uint16_t>(0xFF00u + C), A); return 8;
    case 0xE5: Push16(GetHL()); return 16;
    case 0xE6: AndA(Fetch8()); return 8;
    case 0xE7: return Restart(0x20);
    case 0xE8: { const int8_t e = static_cast<int8_t>(Fetch8()); SP = AddSignedToSP(e); return 16; }
    case 0xE9: PC = GetHL(); return 4;
    case 0xEA: { const uint16_t address = Fetch16(); bus.Write(address, A); return 16; }
    case 0xEE: XorA(Fetch8()); return 8;
    case 0xEF: return Restart(0x28);

    case 0xF0: A = bus.Read(static_cast<uint16_t>(0xFF00u + Fetch8())); return 12;
    case 0xF1: {
        const uint16_t value = Pop16();
        A = static_cast<uint8_t>(value >> 8);
        F = static_cast<uint8_t>(value & 0xF0); // Low nibble of F is always zero.
        return 12;
    }
    case 0xF2: A = bus.Read(static_cast<uint16_t>(0xFF00u + C)); return 8;
    case 0xF3: ime = false; imeEnableDelay = 0; return 4; // DI
    case 0xF5: Push16(static_cast<uint16_t>((static_cast<uint16_t>(A) << 8) | (F & 0xF0))); return 16;
    case 0xF6: OrA(Fetch8()); return 8;
    case 0xF7: return Restart(0x30);
    case 0xF8: { const int8_t e = static_cast<int8_t>(Fetch8()); SetHL(AddSignedToSP(e)); return 12; }
    case 0xF9: SP = GetHL(); return 8;
    case 0xFA: A = bus.Read(Fetch16()); return 16;
    case 0xFB: if (!ime && imeEnableDelay == 0) { imeEnableDelay = 2; } return 4;
    case 0xFE: CpA(Fetch8()); return 8;
    case 0xFF: return Restart(0x38);

    // Undefined LR35902 opcodes: D3 DB DD E3 E4 EB EC ED F4 FC FD.
    default:
        throw std::runtime_error("Undefined opcode");
        return 4;
    }
}

auto CPU::PendingInterrupts() -> uint8_t {
    constexpr uint16_t IE = 0xFFFF;
    constexpr uint16_t IF = 0xFF0F;

    return static_cast<uint8_t>(bus.Read(IE) & bus.Read(IF) & 0x1Fu);
}

auto CPU::ServiceInterrupt(uint8_t pending) -> int {
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
            bus.Read(IF);

        interruptFlags =
            static_cast<uint8_t>(
                interruptFlags
                & static_cast<uint8_t>(~mask)
                );

        bus.Write(IF, interruptFlags);

        // --------------------------------------------
        // Equivalent to CALL interrupt vector
        // --------------------------------------------

        Push16(PC);

        PC = vectors[bit];

        // 5 M-cycles = 20 clock cycles
        return 20;
    }

    return 0;
}

auto CPU::UpdateIME() -> void {
    if (imeEnableDelay == 0) {
        return;
    }

    --imeEnableDelay;

    if (imeEnableDelay == 0) {
        ime = true;
    }
}

} // namespace PixelLink::GameBoy