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

# Use local SDL3 source
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL3
)

# ------------------------------------------------------------
# Game Boy emulator core
# ------------------------------------------------------------

add_library(PixelLinkGameBoy STATIC
    src/GameBoy/Bus.cpp
    src/GameBoy/Cartridge.cpp
    src/GameBoy/CPU.cpp
    src/GameBoy/GameBoy.cpp
    src/GameBoy/Joypad.cpp
    src/GameBoy/PPU.cpp
    src/GameBoy/Timer.cpp
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
    src/Frontend/Emulator.cpp
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
#     src/Main.cpp
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
    tests/GameBoy/Main.cpp
    tests/GameBoy/BusTest.cpp
    tests/GameBoy/CartridgeTest.cpp
    tests/GameBoy/CPUTest.cpp
    tests/GameBoy/InterruptTest.cpp
    tests/GameBoy/JoypadTest.cpp
    tests/GameBoy/PPUTest.cpp
    tests/GameBoy/RTCTest.cpp
    tests/GameBoy/TimerIntegrationTest.cpp
    tests/GameBoy/TimerTest.cpp
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
    tests/Frontend/Main.cpp
    tests/Frontend/EmulatorTest.cpp
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