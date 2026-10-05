## Stage 1: Game Boy Emulator

1. CPU
   - [x] Registers / Opcodes
   - [x] CB Opcodes
   - [x] Flags
   - [x] Stack
   - [x] Timing

2. Interrupt
   - [x] IE / IF
   - [x] IME
   - [x] EI delay
   - [x] HALT wake

3. Cartridge
   - [x] ROM loading
   - [x] Header
   - [x] ROM ONLY

4. Bus
   - [x] Memory map
   - [x] WRAM
   - [x] Echo RAM
   - [x] VRAM storage
   - [x] OAM storage
   - [x] I/O storage
   - [x] HRAM
   - [x] IE

5. Timer
   - [x] DIV
   - [x] TIMA
   - [x] TMA
   - [x] TAC
   - [x] Timer interrupt

6. Emulator Core
   - [x] GameBoy class
   - [x] Master cycle loop
   - [x] GameBoy owns Timer and Joypad
   - [x] Bus maps hardware devices without owning them

7. PPU
   - Part 1: Timing and state
     - [x] PPU timing
     - [x] LY
     - [x] LCD modes 0 / 1 / 2 / 3
     - [x] VBlank interrupt
     - [x] STAT mode bits
     - [x] LY / LYC coincidence flag
   - Part 2: Background
     - [x] Tile data decoding
     - [x] Tile maps
     - [x] SCX / SCY scrolling
     - [x] BGP palette
     - [x] Framebuffer
   - Part 3: Window
     - [x] Window tile map
     - [x] WX / WY positioning
   - Part 4: Sprites
     - [x] OAM scan
     - [x] 10 sprites per scanline
     - [x] 8x8 / 8x16 sprites
     - [x] X / Y flip
     - [x] OBP0 / OBP1
     - [x] Sprite priority
   - Part 5: PPU accuracy
     - [x] STAT interrupts
     - [x] VRAM / OAM access rules
     - [x] Bus-side CPU VRAM / OAM access enforcement
     - [x] Variable mode 3 timing (SCX, window and sprite fetch penalties; selected Mooneye timing ROMs pass)
     - [ ] Pixel FIFO / fetcher if needed

8. DMA
   - Part 1: Bus access model
     - [x] Distinguish CPU / PPU / DMA bus access
     - [x] Enforce CPU VRAM / OAM restrictions through Bus
   - Part 2: OAM DMA (FF46)
     - [x] FF46 register / DMA start
     - [x] Copy 160 bytes to OAM
     - [x] DMA transfer timing
     - [x] CPU bus restrictions during DMA
     - [x] DMA restart behavior
     - [x] DMA tests

9. SDL3 Frontend
   - [x] Window
   - [x] Renderer
   - [x] Display framebuffer
   - [x] Emulator main loop integration
   - [x] Run ROM ONLY games with visible graphics
   - [x] Keyboard input forwarding
   - [x] FrontendTests
   - [x] Emulator / ROM / SDL integration test
   - [x] Standalone PixelLink SDL application (Chinese UI; no test executable required)
   - [x] Recursive assests/roms library; external ROM imports and opens copy into the library
   - [x] ROM selection, search, refresh, native multi-file import and drag/drop
   - [x] Fresh core per game, enter/exit, pause, mute and focus-loss handling
   - [x] Manual/periodic/exit battery saves, automatic loading and visible failure recovery
   - [x] Atomic save replacement and companion save import without overwrites
   - [x] Bounded DesktopTests for the application, local-game PCM and screenshots

10. Joypad
   - [x] FF00
   - [x] P14 / P15 button group selection
   - [x] Active-low button state
   - [x] Keyboard interaction
   - [x] Joypad interrupt
   - [x] Joypad tests

11. Cartridge Controllers
   - [x] Cartridge RAM
   - [x] MBC1
   - [x] MBC3
   - [x] MBC5
   - [x] Save files

12. Accuracy
   - [x] Test ROM runner and initial Mooneye acceptance tests
   - [x] HALT bug
   - [x] HALT, timer read, and OAM DMA boundary timing regressions
   - [x] Broader hardware accuracy suite (40 Mooneye ROMs integrated into GameBoyTests: CPU, PPU, timer, DMA, serial)
   - [x] HardwareAccuracy::run() registered in TestSuites.hpp; argument-free main runs all selected ROMs
   - [x] Shared spdlog test logging (console, timestamped run files, ten-record retention, case duration and failure diagnostics)
   - [x] TIMA/TMA reload-cycle write behavior and FF46 reads during DMA
   - [x] CPU memory access timing within each instruction and DMA start / restart / completion / source rules
   - [x] PPU mode / sprite / SCX timing, LCD on/off and STAT / LYC / VBlank boundaries covered by the selected suite
   - [x] DMG serial transfer engine, interrupt and DMG ABC post-boot clock phase
   - Latest hardware run: 40/40 Mooneye ROMs plus 12/12 Blargg DMG sound ROMs pass; GameBoyTests has 194 passing cases. Debug and Release automatic CTest suites pass. Native Windows video and WASAPI device checks pass through DesktopTests.
     See [HardwareAccuracy.md](HardwareAccuracy.md) for results and the next target.

13. APU
   - [x] Four audio channels: CH1 sweep, CH2 pulse, CH3 wave, CH4 noise
   - [x] Stereo mixer and SDL Audio playback
   - [x] Automatic SDL3.dll deployment for Windows frontend executables
   - [x] Frontend::AudioTest::run() integrated into FrontendTests (SDL dummy driver; restored before the interactive game)
   - [x] Verify local-game stereo PCM and real SDL playback device (Windows WASAPI)
   - [x] Advanced DMG APU accuracy: real DIV falling edges and DIV writes, power/length behavior, trigger quirks, sweep, wave RAM fetch access and retrigger corruption
   - [x] All 12 unmodified Blargg dmg_sound ROMs integrated into GameBoyTests; pinned version and SHA-256 manifest
   - [x] DAC conversion and sample-rate-dependent high-pass filtering
   - Manual listening remains a user/device acceptance check; automated device/PCM checks cannot judge subjective sound quality. Revision-specific analog and zombie-envelope variations are outside the current DMG test set.

## Stage 2: WLAN Remote Controlling

## Stage 3: Bidirectional State Synchronization

## Stage 4: Live Streaming Platform & Game Joining
