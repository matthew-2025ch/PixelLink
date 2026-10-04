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

function(pixellink_add_spdlog)
    # Keep these options local: SDL can stay shared while test logging is static.
    set(BUILD_SHARED_LIBS OFF)
    set(SPDLOG_BUILD_ALL OFF)
    set(SPDLOG_BUILD_SHARED OFF)
    set(SPDLOG_USE_STD_FORMAT ON)
    set(SPDLOG_FMT_EXTERNAL OFF)
    set(SPDLOG_FMT_EXTERNAL_HO OFF)
    set(SPDLOG_BUILD_EXAMPLE OFF)
    set(SPDLOG_BUILD_EXAMPLE_HO OFF)
    set(SPDLOG_BUILD_TESTS OFF)
    set(SPDLOG_BUILD_TESTS_HO OFF)
    set(SPDLOG_BUILD_BENCH OFF)
    set(SPDLOG_INSTALL OFF)
    add_subdirectory(external/spdlog EXCLUDE_FROM_ALL)
endfunction()

pixellink_add_spdlog()

# ------------------------------------------------------------
# Game Boy emulator core
# ------------------------------------------------------------

add_library(PixelLinkGameBoy STATIC
    src/PixelLink/GameBoy/APU.cpp
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
    src/PixelLink/GameBoy/Serial.cpp
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

add_library(PixelLinkTestSupport STATIC
    "src/PixelLink/Test/Logger.cpp"
)
add_library(PixelLink::TestSupport ALIAS PixelLinkTestSupport)
target_include_directories(PixelLinkTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(PixelLinkTestSupport PUBLIC spdlog::spdlog)
target_compile_definitions(PixelLinkTestSupport PRIVATE
    PIXELLINK_TEST_LOG_DIR="${CMAKE_BINARY_DIR}/logs"
)

add_executable(GameBoyTests
    tests/PixelLink/GameBoy/Main.cpp
    tests/PixelLink/GameBoy/APUTest.cpp
    tests/PixelLink/GameBoy/BusTest.cpp
    tests/PixelLink/GameBoy/CartridgeTest.cpp
    tests/PixelLink/GameBoy/CPUTest.cpp
    tests/PixelLink/GameBoy/CoreTimingTest.cpp
    tests/PixelLink/GameBoy/HardwareAccuracy.cpp
    tests/PixelLink/GameBoy/InterruptTest.cpp
    tests/PixelLink/GameBoy/JoypadTest.cpp
    tests/PixelLink/GameBoy/MapperFactoryTest.cpp
    tests/PixelLink/GameBoy/MBC1Test.cpp
    tests/PixelLink/GameBoy/MBC3Test.cpp
    tests/PixelLink/GameBoy/MBC5Test.cpp
    tests/PixelLink/GameBoy/PPUTest.cpp
    tests/PixelLink/GameBoy/RTCTest.cpp
    tests/PixelLink/GameBoy/SaveTest.cpp
    tests/PixelLink/GameBoy/SerialTest.cpp
    tests/PixelLink/GameBoy/TimerIntegrationTest.cpp
    tests/PixelLink/GameBoy/TimerTest.cpp
)

target_link_libraries(GameBoyTests
    PRIVATE
        PixelLink::GameBoy
        PixelLink::TestSupport
)

target_compile_definitions(GameBoyTests
    PRIVATE
        PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME GameBoyTests
    COMMAND GameBoyTests
)

# The hardware suite runs all 40 ROMs inside GameBoyTests and reports every
# failure before returning. Preserve the previous host budget across the set.
set_tests_properties(GameBoyTests PROPERTIES LABELS accuracy TIMEOUT 4800)

# ------------------------------------------------------------
# Emulator integration tests
# ------------------------------------------------------------

add_executable(FrontendTests
    tests/PixelLink/Frontend/Main.cpp
    tests/PixelLink/Frontend/AudioTest.cpp
    tests/PixelLink/Frontend/EmulatorTest.cpp
)

target_link_libraries(FrontendTests
    PRIVATE
        PixelLink::Emulator
        PixelLink::TestSupport
)

target_compile_definitions(FrontendTests
    PRIVATE
        PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME FrontendTests
    COMMAND FrontendTests
)

# Windows searches beside the executable for SDL's runtime DLL. Resolve the
# selected SDL target so this also works with multi-config generators and
# requires no copy when SDL is built as a static library.
function(pixellink_deploy_sdl_runtime target)
    if(WIN32)
        get_target_property(sdl_type SDL3::SDL3 TYPE)
        if(sdl_type STREQUAL "SHARED_LIBRARY")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:SDL3::SDL3>"
                    "$<TARGET_FILE_DIR:${target}>"
                VERBATIM
            )
        endif()
    endif()
endfunction()

pixellink_deploy_sdl_runtime(FrontendTests)
