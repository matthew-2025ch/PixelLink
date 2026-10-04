# Mooneye hardware accuracy ROMs

These 40 ROMs are unmodified binaries from the official
[Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite), release
`mts-20260714-0944-31510e1`. They cover CPU/interrupts, PPU, timer, OAM DMA,
and the DMG ABC/MGB serial clock test. All ROMs use the included [MIT license](LICENSE).

[manifest.json](manifest.json) records the official archive URL, its SHA-256,
and SHA-256 hashes for every checked-in ROM. The suite needs no download at runtime.

## Test organization

All 40 ROMs run inside `GameBoyTests`, through
`PixelLink::Tests::Gameboy::HardwareAccuracy::run()`. The entry is declared
in `include/PixelLink/Test/TestSuites.hpp` and called from the existing
argument-free `main()`. There is no separate hardware accuracy executable.

`tests/PixelLink/GameBoy/HardwareAccuracy.cpp` contains the complete ordered
ROM list. The program selects every listed ROM itself; no command-line
arguments or working-directory-dependent paths are required. A missing ROM
fails its case. Add new acceptance ROMs to this list and the hash manifest.

Each case uses the existing `Test::run` output format and a fresh emulator.
The shared runner in `include/PixelLink/Test/MooneyeRunner.hpp` checks the
official register signature at the `LD B,B` breakpoint. ROM instructions
and hardware reads are unmodified. A failing case prints CPU registers and
PPU/timer/interrupt state; the suite continues through the remaining ROMs
and ultimately returns failure through the main test entry.

Each ROM has a 20-second emulated timeout (83,886,080 T-cycles). CTest provides
a 4,800-second host limit for the combined suite. Audio sample capture is
disabled during these headless tests to bound memory use. The original eight
ROMs are part of this same 40-ROM list and are each run once.

## Running

From the project root in a configured C++ developer environment:

```powershell
cmake --build out/build/hardware --clean-first --target GameBoyTests
out/build/hardware/GameBoyTests.exe
```

Alternatively, run the same combined suite through CTest:

```powershell
ctest --test-dir out/build/hardware -R '^GameBoyTests$' --output-on-failure
```

The `accuracy` label selects `GameBoyTests`; it includes the core tests and
all ROMs. ROMs are reported separately in program output, rather than as
individual CTest entries. For a multi-configuration generator, add
`--config Debug` when building and `-C Debug` when invoking CTest.

## Current limitations

The measured result is **40 pass / 0 fail**, improved from the previous
**26 pass / 14 fail** baseline. See [HardwareAccuracy.md](../../../HardwareAccuracy.md).
This is a DMG-focused subset, not validation of the full Mooneye suite,
CGB/SGB variants, boot state, audio, or manual visual tests.

The sole upstream serial acceptance ROM is `boot_sclk_align-dmgABCmgb`.
PixelLink implements DMG internal serial clocking, disconnected input,
external bit shifting and completion interrupts. The core starts with a
DMG ABC post-boot clock phase; it does not execute a boot ROM. Dedicated
unit tests cover bit timing, abort/restart, external input/output,
DIV independence and interrupt delivery. This single acceptance ROM does
not provide comprehensive serial coverage.

PPU transfer duration accounts for SCX, window and object penalties, but
rendering remains scanline based. A complete pixel FIFO/fetcher and arbitrary
mid-scanline register changes are not validated by this subset.

Windows frontend targets deploy SDL3.dll beside their executables after
linking when SDL is shared. `AudioFrontendTests` uses SDL's dummy audio driver;
audible playback on a physical device remains a separate check.
