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
     - [x] Variable mode 3 timing
     - [x] Pixel FIFO / fetcher if needed

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
   - [ ] Save files

12. Accuracy
   - [ ] Test ROMs
   - [ ] HALT bug
   - [ ] Timing edge cases

13. APU
   - [ ] Audio channels
   - [ ] SDL Audio

## Stage 2: WLAN Remote Controlling

## Stage 3: Bidirectional State Synchronization

## Stage 4: Live Streaming Platform & Game Joining
