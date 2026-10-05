#pragma once
#include <PixelLink/GameBoy/CPU.hpp>

namespace PixelLink::Test::GameBoy {
// Existing instruction/timing tests sometimes need to seed CPU internals.
// Keep this access in test support instead of changing the CPU's public API
// (and class definition) between Debug and Release builds.
struct CPUAccess {
    template<class T> static decltype(auto) A(T& cpu) { return (cpu.A); }
    template<class T> static decltype(auto) F(T& cpu) { return (cpu.F); }
    template<class T> static decltype(auto) B(T& cpu) { return (cpu.B); }
    template<class T> static decltype(auto) C(T& cpu) { return (cpu.C); }
    template<class T> static decltype(auto) H(T& cpu) { return (cpu.H); }
    template<class T> static decltype(auto) L(T& cpu) { return (cpu.L); }
    template<class T> static decltype(auto) PC(T& cpu) { return (cpu.PC); }
    template<class T> static decltype(auto) SP(T& cpu) { return (cpu.SP); }
    template<class T> static decltype(auto) ime(T& cpu) { return (cpu.ime); }
    template<class T> static decltype(auto) halted(T& cpu) { return (cpu.halted); }
};
}
