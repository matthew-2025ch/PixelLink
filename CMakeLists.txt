cmake_minimum_required(VERSION 3.24)

project(
    PixelLink
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------

add_subdirectory(
    external/SDL
    EXCLUDE_FROM_ALL
)

# ------------------------------------------------------------
# Game Boy emulator core
# ------------------------------------------------------------

add_library(PixelLinkGameBoy STATIC
    src/PixelLink/GameBoy/Mapper/ROMOnly.cpp
    src/PixelLink/GameBoy/Mapper/MBC1.cpp
    src/PixelLink/GameBoy/Mapper/MBC3.cpp
    src/PixelLink/GameBoy/Mapper/MBC5.cpp
    src/PixelLink/GameBoy/Bus.cpp
    src/PixelLink/GameBoy/Cartridge.cpp
    src/PixelLink/GameBoy/CPU.cpp
    src/PixelLink/GameBoy/GameBoy.cpp
    src/PixelLink/GameBoy/Joypad.cpp
    src/PixelLink/GameBoy/PPU.cpp
    src/PixelLink/GameBoy/Timer.cpp
)

add_library(PixelLink::GameBoy ALIAS PixelLinkGameBoy)

target_include_directories(PixelLinkGameBoy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Embeddable emulator
# ------------------------------------------------------------

add_library(PixelLinkEmulator STATIC
    src/PixelLink/Frontend/Emulator.cpp
)

add_library(PixelLink::Emulator ALIAS PixelLinkEmulator)

target_include_directories(PixelLinkEmulator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(PixelLinkEmulator
    PUBLIC
        PixelLink::GameBoy
        SDL3::SDL3
)

# ------------------------------------------------------------
# PixelLink application
# ------------------------------------------------------------

# add_executable(PixelLink
#     src/PixelLink/Main.cpp
# )
#
# target_link_libraries(PixelLink
#     PRIVATE
#         PixelLink::Emulator
# )

# ------------------------------------------------------------
# Game Boy tests
# ------------------------------------------------------------

enable_testing()

add_executable(GameBoyTests
    tests/PixelLink/GameBoy/Main.cpp
    tests/PixelLink/GameBoy/BusTest.cpp
    tests/PixelLink/GameBoy/CartridgeTest.cpp
    tests/PixelLink/GameBoy/CPUTest.cpp
    tests/PixelLink/GameBoy/InterruptTest.cpp
    tests/PixelLink/GameBoy/JoypadTest.cpp
    tests/PixelLink/GameBoy/PPUTest.cpp
    tests/PixelLink/GameBoy/RTCTest.cpp
    tests/PixelLink/GameBoy/TimerIntegrationTest.cpp
    tests/PixelLink/GameBoy/TimerTest.cpp
)

target_link_libraries(GameBoyTests
    PRIVATE
        PixelLink::GameBoy
)

add_test(
    NAME GameBoyTests
    COMMAND GameBoyTests
)

# ------------------------------------------------------------
# Emulator integration tests
# ------------------------------------------------------------

add_executable(FrontendTests
    tests/PixelLink/Frontend/Main.cpp
    tests/PixelLink/Frontend/EmulatorTest.cpp
)

target_link_libraries(FrontendTests
    PRIVATE
        PixelLink::Emulator
)

target_compile_definitions(FrontendTests
    PRIVATE
        PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME FrontendTests
    COMMAND FrontendTests
)