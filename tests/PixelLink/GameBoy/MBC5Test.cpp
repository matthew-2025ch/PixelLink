#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/GameBoy/MapperTestUtils.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MBC5Test {

void run() {
    Test::run(
        "MBC5 / RAM access",
        [] {
            MapperTestUtils::TempROM rom(
                "mbc5_test.gb",
                0x1B,
                0x02,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(rom.path());

            cartridge.Write(0x0000, 0x0A);
            cartridge.Write(0xA000, 0x77);

            CHECK(cartridge.Read(0xA000) == 0x77);

        }
    );
}

}
