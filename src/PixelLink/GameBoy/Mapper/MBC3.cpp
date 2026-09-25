#include "PixelLink/GameBoy/Mapper/MBC3.hpp"

namespace PixelLink::GameBoy {

MBC3::MBC3(
    std::vector<uint8_t>& rom,
    std::vector<uint8_t>& ram
)
    : rom_(rom),
      ram_(ram),
      rtcLastUpdate_(std::chrono::steady_clock::now())
{
}

auto MBC3::ReadROM(uint16_t address) const -> uint8_t
{
    std::size_t bank =
        address < 0x4000 ? 0 : romBank_;

    if (address >= 0x4000) {
        address -= 0x4000;
    }

    const std::size_t offset =
        bank * 0x4000 + address;

    return offset < rom_.size()
        ? rom_[offset]
        : 0xFF;
}

auto MBC3::WriteROM(
    uint16_t address,
    uint8_t value
) -> void
{
    if (address <= 0x1FFF) {
        ramEnabled_ =
            (value & 0x0F) == 0x0A;
        return;
    }

    if (address <= 0x3FFF) {
        romBank_ =
            value & 0x7F;

        if (romBank_ == 0) {
            romBank_ = 1;
        }

        return;
    }

    if (address <= 0x5FFF) {
        ramRTCSelect_ = value;
        return;
    }

    if (lastLatchWrite_ == 0 &&
        value == 1) {

        LatchRTC();
    }

    lastLatchWrite_ = value;
}

auto MBC3::ReadRAM(
    uint16_t address
) -> uint8_t
{
    if (!ramEnabled_) {
        return 0xFF;
    }

    if (ramRTCSelect_ >= 0x08 &&
        ramRTCSelect_ <= 0x0C) {

        return ReadRTCRegister(
            ramRTCSelect_
        );
    }

    const std::size_t offset =
        (ramRTCSelect_ & 0x03) * 0x2000 +
        (address - 0xA000);

    return offset < ram_.size()
        ? ram_[offset]
        : 0xFF;
}

auto MBC3::WriteRAM(
    uint16_t address,
    uint8_t value
) -> void
{
    if (!ramEnabled_) {
        return;
    }

    if (ramRTCSelect_ >= 0x08 &&
        ramRTCSelect_ <= 0x0C) {

        WriteRTCRegister(
            ramRTCSelect_,
            value
        );

        return;
    }

    const std::size_t offset =
        (ramRTCSelect_ & 0x03) * 0x2000 +
        (address - 0xA000);

    if (offset < ram_.size()) {
        ram_[offset] = value;
    }
}

auto MBC3::SyncRTC() -> void
{
    if (rtc_.halt) {
        rtcLastUpdate_ =
            std::chrono::steady_clock::now();
        return;
    }

    const auto now =
        std::chrono::steady_clock::now();

    const auto elapsed =
        std::chrono::duration_cast<
            std::chrono::seconds
        >(now - rtcLastUpdate_).count();

    if (elapsed <= 0) {
        return;
    }

    rtcLastUpdate_ +=
        std::chrono::seconds(elapsed);

    uint64_t total =
        rtc_.seconds +
        rtc_.minutes * 60 +
        rtc_.hours * 3600 +
        rtc_.days * 86400 +
        elapsed;

    const uint64_t days =
        total / 86400;

    if (days >= 512) {
        rtc_.carry = true;
    }

    rtc_.days =
        static_cast<uint16_t>(
            days & 0x1FF
        );

    total %= 86400;

    rtc_.hours =
        static_cast<uint8_t>(total / 3600);

    total %= 3600;

    rtc_.minutes =
        static_cast<uint8_t>(total / 60);

    rtc_.seconds =
        static_cast<uint8_t>(total % 60);
}

auto MBC3::LatchRTC() -> void
{
    SyncRTC();

    for (uint8_t reg = 0x08;
         reg <= 0x0C;
         reg++) {

        latchedRTC_[reg - 0x08] =
            RTCRegisterValue(reg);
    }

    rtcLatchedValid_ = true;
}

auto MBC3::ReadRTCRegister(
    uint8_t reg
) -> uint8_t
{
    if (rtcLatchedValid_) {
        return latchedRTC_[reg - 0x08];
    }

    SyncRTC();

    return RTCRegisterValue(reg);
}

auto MBC3::WriteRTCRegister(
    uint8_t reg,
    uint8_t value
) -> void
{
    SyncRTC();

    switch (reg) {
    case 0x08:
        rtc_.seconds = value % 60;
        break;

    case 0x09:
        rtc_.minutes = value % 60;
        break;

    case 0x0A:
        rtc_.hours = value % 24;
        break;

    case 0x0B:
        rtc_.days =
            (rtc_.days & 0x100) | value;
        break;

    case 0x0C:
        rtc_.days =
            (rtc_.days & 0xFF) |
            ((value & 1) << 8);

        rtc_.halt =
            (value & 0x40) != 0;

        rtc_.carry =
            (value & 0x80) != 0;

        rtcLastUpdate_ =
            std::chrono::steady_clock::now();

        break;

    default:
        break;
    }
}

auto MBC3::RTCRegisterValue(
    uint8_t reg
) const -> uint8_t
{
    switch (reg) {
    case 0x08:
        return rtc_.seconds;

    case 0x09:
        return rtc_.minutes;

    case 0x0A:
        return rtc_.hours;

    case 0x0B:
        return rtc_.days & 0xFF;

    case 0x0C:
    {
        uint8_t value =
            (rtc_.days >> 8) & 1;

        if (rtc_.halt)
            value |= 0x40;

        if (rtc_.carry)
            value |= 0x80;

        return value;
    }

    default:
        return 0xFF;
    }
}

}
