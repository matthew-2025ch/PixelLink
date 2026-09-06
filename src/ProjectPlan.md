## Stage1: Game Boy Emulator

1. CPU
   - [x] Registers Opcodes
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
   - [ ] PPU timing
   - [ ] LY
   - [ ] LCD modes
   - [ ] VBlank
   - [ ] Background
   - [ ] Window
   - [ ] Sprites
   - [ ] Framebuffer

8. SDL
   - [ ] Window
   - [ ] Renderer
   - [ ] Display framebuffer

9. Joypad
   - [ ] FF00
   - [ ] Keyboard
   - [ ] Joypad interrupt
10. DMA
    - [ ] OAM DMA

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

## Stage 3: Bidirectional Stage Synchronization

## Stage 4: Live Streaming Platform & Game Joining