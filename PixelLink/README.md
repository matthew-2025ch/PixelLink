# Pixel Link

## Stage 1: GB Emulator Core

## Phase 1 — CPU Core

- [x] Implement CPU registers and flags
- [x] Implement instruction fetch, decode, and execute
- [x] Implement standard opcodes
- [x] Implement CB-prefixed opcodes
- [x] Implement stack operations
- [x] Implement jumps, calls, and returns
- [ ] Add CPU unit tests

## Phase 2 — Interrupt System

- [ ] Implement IE and IF registers
- [ ] Implement interrupt handling
- [ ] Implement VBlank, LCD, Timer, Serial, and Joypad interrupts
- [ ] Implement EI delay
- [ ] Implement HALT behavior

## Phase 3 — Cartridge & Memory

- [ ] Load `.gb` ROM files
- [ ] Parse cartridge headers
- [ ] Implement the Game Boy memory map
- [ ] Connect the cartridge to the memory bus
- [ ] Support ROM-only cartridges
- [ ] Add MBC support later

## Phase 4 — Timer

- [ ] Implement DIV
- [ ] Implement TIMA, TMA, and TAC
- [ ] Implement Timer interrupts
- [ ] Verify timing with test ROMs

## Phase 5 — Graphics

- [ ] Implement VRAM and OAM
- [ ] Implement the PPU state machine
- [ ] Implement background tiles
- [ ] Implement sprites
- [ ] Implement palettes
- [ ] Generate a 160×144 framebuffer

## Phase 6 — Input

- [ ] Implement the Joypad register
- [ ] Map keyboard input to Game Boy buttons
- [ ] Implement Joypad interrupts

## Phase 7 — Game Execution

- [ ] Load real Game Boy ROMs
- [ ] Run CPU, Timer, and PPU together
- [ ] Display the framebuffer with SDL
- [ ] Fix timing and compatibility issues

## Phase 8 — Audio

- [ ] Implement the Game Boy APU
- [ ] Implement sound channels
- [ ] Output audio through SDL

## Phase 9 — Save & Cartridge Controllers

- [ ] Implement cartridge RAM
- [ ] Implement save files
- [ ] Implement MBC1
- [ ] Implement MBC3
- [ ] Implement MBC5

## Stage 2: LAN/WAN Remote Control

## Stage 3: Bidirectional Transmission, State Synchronization

## Stage 4: Live Streaming Platform Adds Gaming Features