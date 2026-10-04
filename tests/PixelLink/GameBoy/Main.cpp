#include <PixelLink/Test/Logger.hpp>
#include <PixelLink/Test/TestSuites.hpp>

using namespace PixelLink::Test::GameBoy;

int main() {
    return PixelLink::Test::RunTestProgram("GameBoyTests", "Game Boy Core Tests", [] {
        APUTest::run();
        BusTest::run();
        CartridgeTest::run();
        CPUTest::run();
        CoreTimingTest::run();
        InterruptTest::run();
        JoypadTest::run();
        MapperFactoryTest::run();
        MBC1Test::run();
        MBC3Test::run();
        MBC5Test::run();
        PPUTest::run();
        RTCTest::run();
        SaveTest::run();
        SerialTest::run();
        TimerIntegrationTest::run();
        TimerTest::run();
        PixelLink::Tests::Gameboy::HardwareAccuracy::run();
    });
}
