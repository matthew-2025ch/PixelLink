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

7. PPU
   - Part 1: Timing and state
     - [x] PPU timing
     - [x] LY
     - [x] LCD modes 0 / 1 / 2 / 3
     - [x] VBlank interrupt
     - [x] STAT mode bits
     - [x] LY / LYC coincidence flag
   - Part 2: Background
     - [ ] Tile data decoding
     - [ ] Tile maps
     - [ ] SCX / SCY scrolling
     - [ ] BGP palette
     - [ ] Framebuffer
   - Part 3: Window
     - [ ] Window tile map
     - [ ] WX / WY positioning
   - Part 4: Sprites
     - [ ] OAM scan
     - [ ] 10 sprites per scanline
     - [ ] 8x8 / 8x16 sprites
     - [ ] X / Y flip
     - [ ] OBP0 / OBP1
     - [ ] Sprite priority
   - Part 5: PPU accuracy
     - [ ] STAT interrupts
     - [ ] VRAM / OAM access restrictions
     - [ ] Variable mode 3 timing
     - [ ] Pixel FIFO / fetcher if needed

8. DMA
   - [ ] OAM DMA (FF46)

9. SDL
   - [ ] Window
   - [ ] Renderer
   - [ ] Display framebuffer

10. Joypad
    - [ ] FF00
    - [ ] Keyboard
    - [ ] Joypad interrupt

11. Cartridge Controllers
    - [ ] Cartridge RAM
    - [ ] MBC1
    - [ ] MBC3
    - [ ] MBC5
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
