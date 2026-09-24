#include <PixelLink/GameBoy/GameBoy.hpp>

namespace PixelLink::GameBoy {

GameBoy::GameBoy()
    : cartridge_(),
      timer_(),
      joypad_(),
      bus_(),
      ppu_(bus_),
      cpu_(bus_) {
    bus_.AttachTimer(timer_);
    bus_.AttachJoypad(joypad_);
}

auto GameBoy::LoadROM(
    const std::filesystem::path& path
) -> void {
    cartridge_.Load(path);
    bus_.InsertCartridge(cartridge_);
}

auto GameBoy::Step() -> int {
    const int tCycles = cpu_.Step();
    const auto elapsed =
        static_cast<std::uint32_t>(tCycles);

    timer_.Tick(elapsed);

    if (timer_.ConsumeInterruptRequest()) {
        bus_.RequestInterrupt(TIMER_INTERRUPT);
    }

    bus_.Tick(elapsed);
    ppu_.Step(elapsed);

    return tCycles;
}

auto GameBoy::SetButton(
    const JoypadButton button,
    const bool pressed
) -> void {
    if (joypad_.SetButton(button, pressed)) {
        bus_.RequestInterrupt(JOYPAD_INTERRUPT);
    }
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

auto GameBoy::GetPPU() noexcept -> PPU& {
    return ppu_;
}

auto GameBoy::GetPPU() const noexcept -> const PPU& {
    return ppu_;
}

auto GameBoy::GetTimer() noexcept -> Timer& {
    return timer_;
}

auto GameBoy::GetTimer() const noexcept -> const Timer& {
    return timer_;
}

auto GameBoy::GetJoypad() noexcept -> Joypad& {
    return joypad_;
}

auto GameBoy::GetJoypad() const noexcept -> const Joypad& {
    return joypad_;
}

} // namespace PixelLink::GameBoy
