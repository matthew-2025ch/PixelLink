#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>

#include "MapperTestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MBC3Test {

void run() {
    Test::run(
        "MBC3 / RAM access",
        [] {
            std::filesystem::path path = "mbc3_ram_test.gb";

            MapperTestUtils::CreateROM(
                path,
                0x13,
                0x01,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(path);

            cartridge.Write(0x0000, 0x0A);
            cartridge.Write(0xA000, 0x66);

            CHECK(cartridge.Read(0xA000) == 0x66);

            std::filesystem::remove(path);
        }
    );
}

}
