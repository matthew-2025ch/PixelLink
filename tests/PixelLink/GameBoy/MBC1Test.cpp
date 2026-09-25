#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>

#include "MapperTestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MBC1Test {

void run() {
    Test::run(
        "MBC1 / RAM enable",
        [] {
            std::filesystem::path path = "mbc1_test.gb";

            MapperTestUtils::CreateROM(
                path,
                0x03,
                0x01,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(path);

            cartridge.Write(0x0000, 0x0A);
            cartridge.Write(0xA000, 0x55);

            CHECK(cartridge.Read(0xA000) == 0x55);

            std::filesystem::remove(path);
        }
    );
}

}
