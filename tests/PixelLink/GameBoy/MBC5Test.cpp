#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>

#include "MapperTestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MBC5Test {

void run() {
    Test::run(
        "MBC5 / RAM access",
        [] {
            std::filesystem::path path = "mbc5_test.gb";

            MapperTestUtils::CreateROM(
                path,
                0x1B,
                0x02,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(path);

            cartridge.Write(0x0000, 0x0A);
            cartridge.Write(0xA000, 0x77);

            CHECK(cartridge.Read(0xA000) == 0x77);

            std::filesystem::remove(path);
        }
    );
}

}
