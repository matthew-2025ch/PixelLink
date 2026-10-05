#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>
#include <PixelLink/Test/GameBoy/MapperTestUtils.hpp>

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MapperFactoryTest {

void run() {
    Test::run(
        "Mapper / Factory creation",
        [] {
            MapperTestUtils::TempROM rom(
                "mapper_Tests.gb",
                0x13,
                0x01,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(rom.path());

            CHECK(cartridge.Loaded());

        }
    );
}

}
