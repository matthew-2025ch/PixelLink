#pragma once

#include "PixelLink/GameBoy/Mapper/Mapper.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace PixelLink::GameBoy {

class MBC3 final : public Mapper {
public:
    MBC3(
        std::vector<uint8_t>& rom,
        std::vector<uint8_t>& ram
    );

    auto ReadROM(uint16_t address) const -> uint8_t override;
    auto WriteROM(uint16_t address, uint8_t value) -> void override;

    auto ReadRAM(uint16_t address) -> uint8_t override;
    auto WriteRAM(uint16_t address, uint8_t value) -> void override;

private:
    struct RTCState {
        uint8_t seconds = 0;
        uint8_t minutes = 0;
        uint8_t hours = 0;
        uint16_t days = 0;

        bool halt = false;
        bool carry = false;
    };

    std::vector<uint8_t>& rom_;
    std::vector<uint8_t>& ram_;

    bool ramEnabled_ = false;

    uint8_t romBank_ = 1;
    uint8_t ramRTCSelect_ = 0;
    uint8_t lastLatchWrite_ = 0xFF;

    RTCState rtc_{};
    std::array<uint8_t, 5> latchedRTC_{};

    bool rtcLatchedValid_ = false;
    std::chrono::steady_clock::time_point rtcLastUpdate_;

    auto SyncRTC() -> void;

    auto LatchRTC() -> void;

    auto ReadRTCRegister(
        uint8_t reg
    ) -> uint8_t;

    auto WriteRTCRegister(
        uint8_t reg,
        uint8_t value
    ) -> void;

    auto RTCRegisterValue(
        uint8_t reg
    ) const -> uint8_t;
};

}
