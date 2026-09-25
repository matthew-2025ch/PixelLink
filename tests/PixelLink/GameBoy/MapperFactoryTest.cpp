#include <filesystem>
#include <PixelLink/GameBoy/Cartridge.hpp>
#include <PixelLink/Test/TestFramework.hpp>

#include "MapperTestUtils.hpp"

using namespace PixelLink::GameBoy;

namespace PixelLink::Test::GameBoy::MapperFactoryTest {

void run() {
    Test::run(
        "Mapper / Factory creation",
        [] {
            std::filesystem::path path = "mapper_test.gb";

            MapperTestUtils::CreateROM(
                path,
                0x13,
                0x01,
                0x03
            );

            Cartridge cartridge;
            cartridge.Load(path);

            CHECK(cartridge.Loaded());

            std::filesystem::remove(path);
        }
    );
}

}
