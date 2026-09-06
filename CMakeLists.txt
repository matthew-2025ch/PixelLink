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
# Game Boy emulator core
# ------------------------------------------------------------

add_library(PixelLinkGameBoy STATIC
	src/GameBoy/Bus.cpp
	src/GameBoy/Cartridge.cpp
	src/GameBoy/CPU.cpp
	src/GameBoy/GameBoy.cpp
	src/GameBoy/Timer.cpp
)

target_include_directories(PixelLinkGameBoy
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Main application
# ------------------------------------------------------------

add_executable(PixelLink
    src/Main.cpp
)

target_link_libraries(PixelLink
    PRIVATE
        PixelLinkGameBoy
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

enable_testing()

add_executable(PixelLinkTests
    tests/GameBoy/TestMain.cpp
    tests/GameBoy/BusTest.cpp
    tests/GameBoy/CartridgeTest.cpp
    tests/GameBoy/CPUTest.cpp
    tests/GameBoy/InterruptTest.cpp
    tests/GameBoy/TimerIntegrationTest.cpp
    tests/GameBoy/TimerTest.cpp
)

target_link_libraries(PixelLinkTests
    PRIVATE
        PixelLinkGameBoy
)

add_test(
    NAME PixelLinkTests
    COMMAND PixelLinkTests
)