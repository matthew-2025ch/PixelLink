#include "GameBoy.hpp"

namespace PixelLink::GameBoy {

GameBoy::GameBoy()
    : bus_(),
    cpu_(bus_) {
}

auto GameBoy::LoadROM(
    const std::filesystem::path& path
) -> void {
    cartridge_.Load(path);
    bus_.InsertCartridge(cartridge_);
}

auto GameBoy::Step() -> int {
    const int tCycles = cpu_.Step();

    bus_.Tick(static_cast<uint32_t>(tCycles));

    return tCycles;
}

auto GameBoy::GetCPU() noexcept -> CPU& {
    return cpu_;
}

auto GameBoy::GetCPU() const noexcept -> const CPU& {
    return cpu_;
}

auto GameBoy::GetBus() noexcept -> Bus& {
    return bus_;
}

auto GameBoy::GetBus() const noexcept -> const Bus& {
    return bus_;
}

auto GameBoy::GetCartridge() noexcept -> Cartridge& {
    return cartridge_;
}

auto GameBoy::GetCartridge() const noexcept
    -> const Cartridge& {
    return cartridge_;
}

} // namespace PixelLink::GameBoy
