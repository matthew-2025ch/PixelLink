#include <PixelLink/GameBoy/GameBoy.hpp>

namespace PixelLink::GameBoy {

GameBoy::GameBoy()
    : cartridge_(),
      timer_(),
      joypad_(),
      apu_(),
      bus_(),
      ppu_(bus_),
      cpu_(bus_) {
    bus_.AttachTimer(timer_);
    bus_.AttachJoypad(joypad_);
    bus_.AttachAPU(apu_);
    cpu_.SetCycleCallback([this](const std::uint32_t cycles) {
        TickDevices(cycles);
        advancedCycles_ += cycles;
    });
}

auto GameBoy::LoadROM(
    const std::filesystem::path& path
) -> void {
    cartridge_.Load(path);
    bus_.InsertCartridge(cartridge_);
}

auto GameBoy::Step() -> int {
    advancedCycles_ = 0;
    const int tCycles = cpu_.Step();
    TickDevices(static_cast<std::uint32_t>(tCycles) - advancedCycles_);

    return tCycles;
}

auto GameBoy::TickDevices(const std::uint32_t elapsed) -> void {
    timer_.Tick(elapsed);

    if (timer_.ConsumeInterruptRequest()) {
        bus_.RequestInterrupt(TIMER_INTERRUPT);
    }

    bus_.Tick(elapsed);
    ppu_.Step(elapsed);
    apu_.Tick(elapsed);
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

auto GameBoy::GetAPU() noexcept -> APU& {
    return apu_;
}

auto GameBoy::GetAPU() const noexcept -> const APU& {
    return apu_;
}

} // namespace PixelLink::GameBoy
