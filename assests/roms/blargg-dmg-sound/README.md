# Blargg DMG sound hardware tests

These twelve original, unmodified Game Boy test ROMs are by Shay Green (Blargg).
Source: [retrio/gb-test-roms, dmg_sound](https://github.com/retrio/gb-test-roms/tree/c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15/dmg_sound).
Pinned upstream commit: `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15`.

`manifest.json` records each upstream download URL and SHA-256. No ROM was
patched, reassembled or selected for a special path in the emulator.

`GameBoyTests` runs all twelve through `APUAccuracyTest::run()`. The runner
executes the normal CPU and observes Blargg's documented memory result protocol:
signature `DE B0 61` at A001–A003, status at A000 (80 = running, 0 = passed),
and diagnostics at A004. A missing ROM, timeout or nonzero result fails the suite;
remaining ROMs are still run. Each case has a 30 emulated-second cycle budget.

Each ROM is copied to a fresh directory under the build's `apu-tests/` before
execution. Its generated battery save is removed afterwards; test runs do not
load or modify saves in the user's ROM library.

Coverage: register masks, lengths, triggers, sweep and overflow, divider/power
phase, DMG power-off length writes, wave RAM reads/writes, wave retrigger
corruption and registers after power cycling. These tests observe CPU-visible
hardware behavior; they do not assess subjective sound quality or every chip
revision's analog behavior.
