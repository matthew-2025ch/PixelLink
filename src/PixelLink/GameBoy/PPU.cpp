#include <PixelLink/GameBoy/PPU.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>

namespace PixelLink::GameBoy {

PPU::PPU(Bus& bus)
    : bus_(bus) {
    bus_.AttachPPU(*this);
    Reset();
}

PPU::~PPU() {
    bus_.DetachPPU(*this);
}

void PPU::Reset() {
    lineDot_ = 0;
    physicalLine_ = 0;
    firstLineAfterEnable_ = false;
    windowLine_ = 0;
    statInterruptLine_ = false;

    framebuffer_.fill(0);
    ResetPipeline();
    windowYTriggered_ = ReadBus(WY) == 0;

    SetLY(0);

    if (IsLCDEnabled()) {
        SetMode(Mode::OAMScan);
    } else {
        SetMode(Mode::HBlank);
    }

    UpdateSTAT();
}

void PPU::Step(const std::uint32_t tCycles) {
    for (std::uint32_t i = 0; i < tCycles; ++i) {
        StepOneDot();
    }
}

PPU::Mode PPU::GetMode() const noexcept {
    return mode_;
}

std::uint8_t PPU::GetLY() const noexcept {
    return ly_;
}

std::uint16_t PPU::GetLineDot() const noexcept {
    return lineDot_;
}

std::uint8_t PPU::GetWindowLine() const noexcept {
    return windowLine_;
}

const PPU::Framebuffer& PPU::GetFramebuffer() const noexcept {
    return framebuffer_;
}

bool PPU::CanCPUAccessVRAM(const bool write) const noexcept {
    if (!IsLCDEnabled()) {
        return true;
    }

    // On ordinary lines the read port closes two dots before the STAT
    // Mode 3 transition. Writes close at the mode transition itself.
    if (!write && !firstLineAfterEnable_ && physicalLine_ < VISIBLE_LINES &&
        lineDot_ >= OAM_SCAN_END - 2 && lineDot_ < OAM_SCAN_END) {
        return false;
    }
    return mode_ != Mode::Drawing;
}

bool PPU::CanCPUAccessOAM(const bool write) const noexcept {
    if (!IsLCDEnabled()) {
        return true;
    }

    if (!write && physicalLine_ < VISIBLE_LINES &&
        !firstLineAfterEnable_ && lineDot_ < 4) {
        return false;
    }

    // The OAM write gate briefly opens at the end of the search, before
    // STAT reports Mode 3. The OAM read port remains closed throughout.
    if (write && mode_ == Mode::OAMScan &&
        lineDot_ >= OAM_SCAN_END - 2 && lineDot_ < OAM_SCAN_END) {
        return true;
    }

    // OAM is blocked during Modes 2 and 3.
    return mode_ == Mode::HBlank ||
           mode_ == Mode::VBlank;
}

bool PPU::IsLCDEnabled() const {
    return (ReadBus(LCDC) & 0x80u) != 0;
}

std::uint8_t PPU::ReadBus(
    const std::uint16_t address
) const {
    return bus_.Read(address, BusAccess::PPU);
}

void PPU::WriteBus(
    const std::uint16_t address,
    const std::uint8_t value
) {
    bus_.Write(address, value, BusAccess::PPU);
}

void PPU::OnRegisterWrite(const std::uint16_t address, const std::uint8_t oldValue) {
    if (address == LCDC && ((oldValue ^ ReadBus(LCDC)) & 0x80) != 0) {
        lineDot_ = 0;
        physicalLine_ = 0;
        ly_ = 0;
        WriteBus(LY, 0);
        windowLine_ = 0;
        ResetPipeline();
        windowYTriggered_ = ReadBus(WY) == 0;
        if (!IsLCDEnabled()) framebuffer_.fill(0);
        mode_ = Mode::HBlank;
        firstLineAfterEnable_ = IsLCDEnabled();
    }
    UpdateSTAT();
}

void PPU::ResetFetcher(const bool discardFirstTile) {
    fetchPhase_ = FetchPhase::Tile;
    fetchDot_ = 0;
    fetchX_ = 0;
    backgroundHead_ = backgroundSize_ = 0;
    discardFirstTile_ = discardFirstTile;
}

void PPU::ResetPipeline() {
    ResetFetcher(true);
    objectFIFO_.fill({});
    objectHead_ = 0;
    spriteCount_ = nextSprite_ = 0;
    objectFetchActive_ = false;
    spriteFetchDot_ = 0;
    spriteWait_ = 0;
    previousSpriteTile_ = -1;
    firstObjectFetch_ = true;
    outputX_ = 0;
    startupDots_ = 0;
    discardPixels_ = fineScroll_ = 0;
    windowActive_ = windowRendered_ = false;
    windowLeft_ = 0;
}

void PPU::ScanOAM() {
    if (mode_ == Mode::Drawing) return;
    const int start = firstLineAfterEnable_ ? 0 : 4;
    const int elapsed = lineDot_ - start;
    if (elapsed < 2 || elapsed > 80 || (elapsed & 1) != 0) return;
    const auto index = static_cast<std::uint8_t>(elapsed / 2 - 1);
    const auto address = static_cast<std::uint16_t>(OAM_BASE + index * 4);
    const auto y = ReadBus(address);
    const auto x = ReadBus(address + 1);
    const int height = (ReadBus(LCDC) & 4) != 0 ? 16 : 8;
    const int row = static_cast<int>(ly_) + 16 - y;
    if (row >= 0 && row < height && spriteCount_ < lineSprites_.size()) {
        lineSprites_[spriteCount_++] = Sprite{.oamIndex = index, .y = y, .x = x};
    }
}

void PPU::BeginDrawing() {
    ResetFetcher(true);
    fineScroll_ = ReadBus(SCX) & 7;
    discardPixels_ = fineScroll_;
    startupDots_ = 12;
    std::sort(lineSprites_.begin(), lineSprites_.begin() + spriteCount_,
        [](const Sprite& a, const Sprite& b) {
            return a.x != b.x ? a.x < b.x : a.oamIndex < b.oamIndex;
        });
    mode_ = Mode::Drawing;
}

bool PPU::PushBackgroundPixels() {
    if (discardFirstTile_) {
        discardFirstTile_ = false;
        return true;
    }
    if (backgroundSize_ > 8) return false;
    for (int bit = 7; bit >= 0; --bit) {
        const auto tail = (backgroundHead_ + backgroundSize_) % backgroundFIFO_.size();
        backgroundFIFO_[tail] = static_cast<std::uint8_t>(
            ((fetchLow_ >> bit) & 1) | (((fetchHigh_ >> bit) & 1) << 1));
        ++backgroundSize_;
    }
    ++fetchX_;
    return true;
}

void PPU::StepFetcher() {
    if (fetchPhase_ == FetchPhase::Push) {
        if (PushBackgroundPixels()) {
            fetchPhase_ = FetchPhase::Tile;
            fetchDot_ = 0;
        }
        return;
    }
    if (++fetchDot_ < 2) return;
    fetchDot_ = 0;
    const auto lcdc = ReadBus(LCDC);
    switch (fetchPhase_) {
    case FetchPhase::Tile: {
        const auto y = windowActive_ ? windowLine_ :
            static_cast<std::uint8_t>(ly_ + ReadBus(SCY));
        const auto x = windowActive_ ? fetchX_ :
            static_cast<std::uint8_t>((ReadBus(SCX) / 8 + fetchX_) & 31);
        const auto map = windowActive_ ? GetWindowTileMapBase(lcdc) : GetBackgroundTileMapBase(lcdc);
        fetchTile_ = ReadBus(static_cast<std::uint16_t>(map + (y / 8) * 32 + (x & 31)));
        fetchPhase_ = FetchPhase::Low;
        break;
    }
    case FetchPhase::Low:
    case FetchPhase::High: {
        // DMG samples SCY and LCDC's tile-data selection on each plane read.
        const auto y = windowActive_ ? windowLine_ :
            static_cast<std::uint8_t>(ly_ + ReadBus(SCY));
        fetchAddress_ = static_cast<std::uint16_t>(GetTileDataAddress(fetchTile_, lcdc) + (y & 7) * 2);
        if (fetchPhase_ == FetchPhase::Low) {
            fetchLow_ = ReadBus(fetchAddress_);
            fetchPhase_ = FetchPhase::High;
        } else {
            fetchHigh_ = ReadBus(fetchAddress_ + 1);
            // First fetch is discarded; the visible fetch starts immediately.
            const bool first = discardFirstTile_;
            if (PushBackgroundPixels()) {
                fetchPhase_ = first ? FetchPhase::Tile : FetchPhase::Sleep;
            } else {
                fetchPhase_ = FetchPhase::Push;
            }
        }
        break;
    }
    case FetchPhase::Sleep:
        fetchPhase_ = FetchPhase::Tile;
        break;
    case FetchPhase::Push:
        break;
    }
}

bool PPU::StartObjectFetch() {
    while (nextSprite_ < spriteCount_) {
        const auto& sprite = lineSprites_[nextSprite_];
        if (sprite.x >= 168 || static_cast<int>(sprite.x) - 8 > outputX_) return false;
        ++nextSprite_;
        if ((ReadBus(LCDC) & 2) == 0) continue;
        fetchingSprite_ = sprite;
        const int position = windowActive_ ? static_cast<int>(outputX_) - windowLeft_ : sprite.x + fineScroll_;
        const int tile = position / 8;
        spriteWait_ = tile != previousSpriteTile_ ? std::max(5 - (position & 7), 0) : 0;
        previousSpriteTile_ = tile;
        // The first object fetch shares its first dot with the BG handoff.
        spriteFetchDot_ = firstObjectFetch_ ? 1 : 0;
        firstObjectFetch_ = false;
        objectFetchActive_ = true;
        return true;
    }
    return false;
}

void PPU::StepObjectFetcher() {
    if ((ReadBus(LCDC) & 2) == 0) {
        objectFetchActive_ = false;
        return;
    }
    if (spriteWait_ > 0) {
        --spriteWait_;
        StepFetcher();
        return;
    }
    ++spriteFetchDot_;
    if (spriteFetchDot_ == 2) {
        const auto address = static_cast<std::uint16_t>(OAM_BASE + fetchingSprite_.oamIndex * 4);
        fetchingSprite_.tileId = ReadBus(address + 2);
        fetchingSprite_.attributes = ReadBus(address + 3);
        const int height = (ReadBus(LCDC) & 4) != 0 ? 16 : 8;
        int row = static_cast<int>(ly_) + 16 - fetchingSprite_.y;
        if ((fetchingSprite_.attributes & 0x40) != 0) row = height - 1 - row;
        const int tile = height == 16 ? (fetchingSprite_.tileId & 0xFE) + row / 8 : fetchingSprite_.tileId;
        spriteAddress_ = static_cast<std::uint16_t>(TILE_DATA_UNSIGNED_BASE + tile * 16 + (row & 7) * 2);
    } else if (spriteFetchDot_ == 4) {
        spriteLow_ = ReadBus(spriteAddress_);
    } else if (spriteFetchDot_ == 6) {
        spriteHigh_ = ReadBus(spriteAddress_ + 1);
        MergeObjectPixels();
        objectFetchActive_ = false;
    }
}

void PPU::MergeObjectPixels() {
    const int left = static_cast<int>(fetchingSprite_.x) - 8;
    for (int column = 0; column < 8; ++column) {
        const int offset = left + column - outputX_;
        if (offset < 0 || offset >= 8) continue;
        const int bit = (fetchingSprite_.attributes & 0x20) != 0 ? column : 7 - column;
        const auto color = static_cast<std::uint8_t>(
            ((spriteLow_ >> bit) & 1) | (((spriteHigh_ >> bit) & 1) << 1));
        auto& pixel = objectFIFO_[(objectHead_ + offset) % objectFIFO_.size()];
        if (pixel.color == 0 && color != 0) pixel = {color, fetchingSprite_.attributes};
    }
}

void PPU::OutputPixel() {
    auto background = backgroundFIFO_[backgroundHead_];
    backgroundHead_ = (backgroundHead_ + 1) % backgroundFIFO_.size();
    --backgroundSize_;
    if (discardPixels_ != 0) {
        --discardPixels_;
        return;
    }
    const auto lcdc = ReadBus(LCDC);
    if ((lcdc & 1) == 0) background = 0;
    const auto object = objectFIFO_[objectHead_];
    objectFIFO_[objectHead_] = {};
    objectHead_ = (objectHead_ + 1) % objectFIFO_.size();
    const bool showObject = (lcdc & 2) != 0 && object.color != 0 &&
        ((object.attributes & 0x80) == 0 || background == 0);
    const auto shade = showObject
        ? ApplyPalette(object.color, ReadBus((object.attributes & 0x10) != 0 ? OBP1 : OBP0))
        : ApplyPalette(background, ReadBus(BGP));
    framebuffer_[static_cast<std::size_t>(ly_) * SCREEN_WIDTH + outputX_] = shade;
    windowRendered_ |= windowActive_;
    if (++outputX_ == SCREEN_WIDTH) {
        if (windowRendered_) ++windowLine_;
        mode_ = Mode::HBlank;
    }
}

void PPU::StepDrawing() {
    if (startupDots_ != 0) {
        StepFetcher();
        --startupDots_;
        return;
    }
    if (objectFetchActive_) {
        StepObjectFetcher();
        return;
    }
    // The window restarts the BG fetcher at its on-screen edge, costing six dots.
    const auto lcdc = ReadBus(LCDC);
    const int left = static_cast<int>(ReadBus(WX)) - 7;
    if (windowActive_ && (lcdc & 0x21) != 0x21) {
        // Resume the background at the current screen coordinate, not tile zero.
        windowActive_ = false;
        ResetFetcher(false);
        const int backgroundX = outputX_ + ReadBus(SCX);
        fetchX_ = static_cast<std::uint8_t>((outputX_ + (ReadBus(SCX) & 7)) / 8);
        discardPixels_ = static_cast<std::uint8_t>(backgroundX & 7);
    }
    if (!windowActive_ && discardPixels_ == 0 && windowYTriggered_ &&
        (lcdc & 0x21) == 0x21 && left < 160 && outputX_ == std::max(left, 0)) {
        windowActive_ = true;
        windowLeft_ = left;
        previousSpriteTile_ = -1;
        ResetFetcher(false);
        discardPixels_ = static_cast<std::uint8_t>(std::max(-left, 0));
    }
    if (discardPixels_ == 0 && StartObjectFetch()) {
        StepObjectFetcher();
        return;
    }
    if (backgroundSize_ != 0) OutputPixel();
    if (mode_ == Mode::Drawing) StepFetcher();
}

void PPU::StepOneDot() {
    if (!IsLCDEnabled()) {
        return; // the coincidence flag and STAT line freeze with the LCD clock
    }
    ++lineDot_;
    if (physicalLine_ < VISIBLE_LINES) {
        ScanOAM();
        if (!firstLineAfterEnable_ && lineDot_ == 4) {
            mode_ = Mode::OAMScan;
            windowYTriggered_ |= ly_ == ReadBus(WY);
        }
        const auto mode3Start = firstLineAfterEnable_ ? 78 : OAM_SCAN_END;
        if (lineDot_ == mode3Start) {
            BeginDrawing();
        } else if (mode_ == Mode::Drawing) {
            StepDrawing();
        }
    }
    if (physicalLine_ == VISIBLE_LINES && lineDot_ == 4) {
        mode_ = Mode::VBlank;
        RequestVBlankInterrupt();
    }
    // LY becomes zero at dot 4 of the final VBlank line, but Mode 1
    // continues until the physical end of line 153.
    if (physicalLine_ == 153 && lineDot_ == 4) {
        ly_ = 0;
        WriteBus(LY, 0);
    }
    const auto lineLength = firstLineAfterEnable_ ? 450 : DOTS_PER_LINE;
    if (lineDot_ >= lineLength) {
        lineDot_ = 0;
        firstLineAfterEnable_ = false;
        physicalLine_ = static_cast<std::uint8_t>((physicalLine_ + 1) % TOTAL_LINES);
        ly_ = physicalLine_;
        WriteBus(LY, ly_);
        if (physicalLine_ == 0) {
            windowLine_ = 0;
            windowYTriggered_ = false;
        }
        ResetPipeline();
        if (physicalLine_ > VISIBLE_LINES) {
            mode_ = Mode::VBlank;
        } else {
            mode_ = Mode::HBlank;
        }
    }
    UpdateSTAT();
}

void PPU::SetMode(const Mode mode) {
    mode_ = mode;
    UpdateSTAT();
}

void PPU::SetLY(const std::uint8_t value) {
    ly_ = value;
    WriteBus(LY, ly_);
    UpdateSTAT();
}

void PPU::UpdateSTAT() {
    const bool lcdEnabled = IsLCDEnabled();

    // Bits 3-6 are writable interrupt-select bits.
    // Bit 7 reads as 1 on DMG.
    // Bits 0-2 are produced by the PPU.
    std::uint8_t stat = static_cast<std::uint8_t>(
        (ReadBus(STAT) & (lcdEnabled ? 0x78u : 0x7Cu)) | 0x80u
    );

    if (lcdEnabled) {
        stat = static_cast<std::uint8_t>(
            stat |
            static_cast<std::uint8_t>(mode_)
        );

        const bool comparisonRunning = firstLineAfterEnable_ ||
            physicalLine_ >= VISIBLE_LINES || lineDot_ >= 4;
        if (comparisonRunning && ly_ == ReadBus(LYC)) {
            stat = static_cast<std::uint8_t>(
                stat | 0x04u
            );
        }
    }

    WriteBus(STAT, stat);
    UpdateSTATInterruptLine();
}

void PPU::UpdateSTATInterruptLine() {
    if (!IsLCDEnabled()) {
        return;
    }
    bool interruptLine = false;

    if (IsLCDEnabled()) {
        const std::uint8_t stat = ReadBus(STAT);

        const bool lycSource =
            (stat & 0x40u) != 0 &&
            (stat & 0x04u) != 0;

        const bool mode2Source =
            (stat & 0x20u) != 0 &&
            (mode_ == Mode::OAMScan ||
             (physicalLine_ == 144 && lineDot_ == 4));

        const bool mode1Source =
            (stat & 0x10u) != 0 &&
            mode_ == Mode::VBlank;

        const bool mode0Source =
            (stat & 0x08u) != 0 &&
            mode_ == Mode::HBlank;

        interruptLine =
            lycSource ||
            mode2Source ||
            mode1Source ||
            mode0Source;
    }

    if (interruptLine && !statInterruptLine_) {
        RequestSTATInterrupt();
    }

    statInterruptLine_ = interruptLine;
}

void PPU::RequestVBlankInterrupt() {
    const std::uint8_t interruptFlags = ReadBus(IF);

    WriteBus(
        IF,
        static_cast<std::uint8_t>(interruptFlags | 0x01u)
    );
}

void PPU::RequestSTATInterrupt() {
    const std::uint8_t interruptFlags = ReadBus(IF);

    WriteBus(
        IF,
        static_cast<std::uint8_t>(interruptFlags | 0x02u)
    );
}

std::uint16_t PPU::GetBackgroundTileMapBase(
    const std::uint8_t lcdc
) const noexcept {
    return (lcdc & 0x08u) != 0
        ? BG_TILE_MAP_1_BASE
        : BG_TILE_MAP_0_BASE;
}

std::uint16_t PPU::GetWindowTileMapBase(
    const std::uint8_t lcdc
) const noexcept {
    return (lcdc & 0x40u) != 0
        ? BG_TILE_MAP_1_BASE
        : BG_TILE_MAP_0_BASE;
}

std::uint16_t PPU::GetTileDataAddress(
    const std::uint8_t tileId,
    const std::uint8_t lcdc
) const noexcept {
    if ((lcdc & 0x10u) != 0) {
        return static_cast<std::uint16_t>(
            TILE_DATA_UNSIGNED_BASE +
            static_cast<std::uint16_t>(tileId) * 16u
        );
    }

    const std::int16_t signedTileId =
        tileId < 0x80u
            ? static_cast<std::int16_t>(tileId)
            : static_cast<std::int16_t>(
                static_cast<std::int16_t>(tileId) - 256
            );

    const std::int32_t address =
        static_cast<std::int32_t>(TILE_DATA_SIGNED_BASE) +
        static_cast<std::int32_t>(signedTileId) * 16;

    return static_cast<std::uint16_t>(address);
}

std::uint8_t PPU::ApplyPalette(
    const std::uint8_t colorId,
    const std::uint8_t palette
) noexcept {
    const std::uint8_t shift =
        static_cast<std::uint8_t>(colorId * 2u);

    return static_cast<std::uint8_t>(
        (palette >> shift) & 0x03u
    );
}

} // namespace PixelLink::GameBoy
