#include <cstdint>

#include <PixelLink/GameBoy/Bus.hpp>
#include <PixelLink/GameBoy/GameBoy.hpp>
#include <PixelLink/GameBoy/Joypad.hpp>
#include <PixelLink/Test/TestFramework.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::JoypadTest {

namespace {

constexpr std::uint16_t JOYP = 0xFF00;
constexpr std::uint16_t IF = 0xFF0F;
constexpr std::uint8_t JOYPAD_INTERRUPT = 1u << 4;

void testInitialState() {
    Joypad joypad;

    CHECK(joypad.Read() == 0xFF);
}

void testDirectionSelection() {
    Joypad joypad;

    CHECK(!joypad.Write(0x20));
    CHECK(joypad.Read() == 0xEF);

    CHECK(joypad.SetButton(JoypadButton::Right, true));
    CHECK(joypad.Read() == 0xEE);

    CHECK(!joypad.SetButton(JoypadButton::Right, false));
    CHECK(joypad.Read() == 0xEF);
}

void testActionSelection() {
    Joypad joypad;

    CHECK(!joypad.Write(0x10));
    CHECK(joypad.Read() == 0xDF);

    CHECK(joypad.SetButton(JoypadButton::A, true));
    CHECK(joypad.Read() == 0xDE);

    CHECK(!joypad.SetButton(JoypadButton::A, false));
    CHECK(joypad.Read() == 0xDF);
}

void testUnselectedButtonDoesNotRequestInterrupt() {
    Joypad joypad;

    // Select directions only.
    CHECK(!joypad.Write(0x20));

    // A is physically pressed, but its group is not selected.
    CHECK(!joypad.SetButton(JoypadButton::A, true));

    // Selecting the action group exposes the already-held A button,
    // producing the P10 high-to-low transition.
    CHECK(joypad.Write(0x10));
    CHECK(joypad.Read() == 0xDE);
}

void testGameBoyJoypadInterrupt() {
    ::PixelLink::GameBoy::GameBoy gameBoy;
    Bus& bus = gameBoy.GetBus();

    bus.Write(IF, 0x00);
    bus.Write(JOYP, 0x20); // Select directions.

    gameBoy.SetButton(JoypadButton::Right, true);

    CHECK(bus.Read(JOYP) == 0xEE);
    CHECK((bus.Read(IF) & JOYPAD_INTERRUPT) != 0);

    // Releasing a key is a low-to-high transition and does not request
    // another Joypad interrupt.
    bus.Write(IF, 0x00);
    gameBoy.SetButton(JoypadButton::Right, false);

    CHECK(bus.Read(JOYP) == 0xEF);
    CHECK((bus.Read(IF) & JOYPAD_INTERRUPT) == 0);
}

void testJoypWriteCanRequestInterrupt() {
    ::PixelLink::GameBoy::GameBoy gameBoy;
    Bus& bus = gameBoy.GetBus();

    // No group selected initially, so pressing A does not yet pull a
    // selected input line low.
    bus.Write(IF, 0x00);
    gameBoy.SetButton(JoypadButton::A, true);

    CHECK((bus.Read(IF) & JOYPAD_INTERRUPT) == 0);

    // Selecting the action group exposes the held A button.
    bus.Write(JOYP, 0x10);

    CHECK(bus.Read(JOYP) == 0xDE);
    CHECK((bus.Read(IF) & JOYPAD_INTERRUPT) != 0);
}

} // namespace

void run() {
    Test::run("Joypad / initial state", testInitialState);
    Test::run("Joypad / direction selection", testDirectionSelection);
    Test::run("Joypad / action selection", testActionSelection);
    Test::run(
        "Joypad / unselected button interrupt",
        testUnselectedButtonDoesNotRequestInterrupt
    );
    Test::run(
        "Joypad / GameBoy interrupt request",
        testGameBoyJoypadInterrupt
    );
    Test::run(
        "Joypad / JOYP write interrupt",
        testJoypWriteCanRequestInterrupt
    );
}

} // namespace PixelLink::Test::GameBoy::JoypadTest
