# Mooneye accuracy ROMs

These eight ROMs come from the [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite),
prebuilt release `mts-20260714-0944-31510e1`. They are distributed under
the included [MIT license](LICENSE).

The checked-in subset covers interrupt enable sequencing, HALT behavior,
DIV timing, and instruction fetches that cross the OAM DMA completion boundary.
It is an initial accuracy regression set, not a claim that the full Mooneye
acceptance suite passes.

`TestRomRunner` is part of the `GameBoyTests` executable and runs all eight
ROMs without opening a window. It checks the Mooneye register signature at
the `LD B,B` breakpoint. A reported failure, missing ROM, or timeout fails
the test suite. The ROM paths are resolved from the project root, so the
tests work from any working directory.

For example, from the project root after configuring and building the
`local-msvc` Debug preset:

```powershell
ctest --test-dir out/build/local-msvc -C Debug -R '^GameBoyTests$' --output-on-failure
```

To add another Mooneye ROM to the automatic suite, place it in the
`acceptance` directory and add its base name to `ROM_NAMES` in
`tests/PixelLink/GameBoy/TestRomRunner.cpp`.
