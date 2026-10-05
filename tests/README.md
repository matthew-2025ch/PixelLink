# Test programs and logging

`GameBoyTests` runs the core suites, all 40 selected Mooneye ROMs, and all
12 original Blargg DMG sound ROMs through `APUAccuracyTest::run()`.
`DesktopTests` runs bounded, automatic SDL audio and application tests: recursive
ROM discovery, Unicode paths, safe imports, game controls, saves and save-failure
recovery, local-game PCM, and screenshots. It uses dummy video/audio drivers;
`DesktopTests --real-audio` selects the real default audio device while keeping
the test window hidden.
`FrontendTests` runs the automatic audio check, then opens the interactive
game test. Close the game window to finish the frontend suite.

Desktop test sources live in `tests/PixelLink/DesktopTests/`. Each suite exposes
`PixelLink::Tests::Desktop::xxxTest::run()`, declared in
`include/PixelLink/Test/TestSuites.hpp` and called from `DesktopTests/Main.cpp`:
`AudioTest`, `ROMLibraryTest`, `ApplicationTest`, `LocalGameTest`, and
`LibraryPreviewTest`. `TestUtils` shares fixtures, ROM creation, key events and
device settings. `AudioTest` reuses the frontend's automatic audio check.

All three entry points use `PixelLink::Test::RunTestProgram`. Test cases keep the
existing `Test::run` and `CHECK` interfaces. `CHECK` throws on failure;
`Test::run` logs the case and rethrows. The main wrapper reports the final
result, flushes logging and returns 0 on success or 1 on failure.

## Dependency

spdlog is a Git submodule at `external/spdlog`, alongside `external/SDL`.
It is pinned to **v1.17.0**, commit
`79524ddd08a4ec981b7fea76afd08ee05f83755d`. The repository URL is recorded
in `.gitmodules`, and the parent project's Git link records the exact commit.
Initialize both dependencies with `git submodule update --init --recursive`.
CMake builds the checked-out dependency through `add_subdirectory`.

spdlog is compiled as a static library with `std::format`; there is no extra
logging DLL or external fmt dependency. The shared `PixelLink::TestSupport`
target links it for the test programs. SDL's shared/static setting is configured
separately.

## Output

- Synchronous, colored console logging keeps `[PASS]` and `[FAIL]` markers.
- Every case reports its elapsed time in milliseconds.
- Failure output includes the existing assertion/ROM diagnostics.
- Console and file output share severity and local-time formatting:

  ```text
  [info 2026-10-05 14:30:05:123] [PASS] CPU / example (0.15 ms)
  [warning 2026-10-05 14:30:05:124] Example warning
  [error 2026-10-05 14:30:05:125] [FAIL] CPU / example
  ```

- The final three timestamp digits are milliseconds.
- Info-level results, warnings and errors flush immediately; each program
  also flushes before returning.

Logs go to `logs/` under the configured build directory, regardless of the
working directory used to launch the executable:

```text
out/build/hardware/logs/[2026-10-05 14-30-05-123] GameBoyTests.log
out/build/hardware/logs/[2026-10-05 14-31-12-456] FrontendTests.log
```

Each executable creates a new file named after its local start time.
Windows filenames use hyphens in place of time colons. An existing filename
advances the filename timestamp by one millisecond to select a fresh record.

The directory retains the newest **10 timestamped run logs in total**, shared
by `GameBoyTests`, `FrontendTests` and `DesktopTests`. Opening a new log deletes the oldest
managed records beyond that limit. The currently opened log is retained;
cleanup errors are reported as warnings. The managed files are the three test
programs' timestamped `.log` records.

The default level is `info`. Set `SPDLOG_LEVEL` to change it without
command-line arguments:

```powershell
$env:SPDLOG_LEVEL = 'debug'
out/build/hardware/GameBoyTests.exe
Remove-Item Env:SPDLOG_LEVEL
```

Additional case diagnostics can use the shared logger:

```cpp
PixelLink::Test::GetLogger().debug("PC={:04X}, cycles={}", pc, cycles);
```

Changing the log level filters output; assertions and process exit status
still determine whether the tests passed. File logging is initialized before
the suites; a file setup error is reported to the console and returns failure.

## Building

Initialize the SDL and spdlog submodules, then configure and build in a C++20
developer environment:

```powershell
git submodule update --init --recursive
cmake -S . -B out/build/hardware -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/hardware --target PixelLink GameBoyTests DesktopTests FrontendTests
ctest --test-dir out/build/hardware -R '^(GameBoyTests|DesktopTests)$' --output-on-failure
```

For a multi-configuration generator, use `--config Debug` when building.
The frontend target deploys SDL3.dll beside its executable on Windows when
SDL is shared.
