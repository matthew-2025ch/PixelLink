cmake_minimum_required(VERSION 3.24)

project(
    PixelLink
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # UTF-8 source and UI strings, independent of the developer's code page.
    add_compile_options(/utf-8)
endif()
set(PIXELLINK_MSVC_INCLUDE_PREFIX "" CACHE STRING "Optional localized MSVC /showIncludes prefix for Ninja")
if(MSVC AND NOT PIXELLINK_MSVC_INCLUDE_PREFIX STREQUAL "")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${PIXELLINK_MSVC_INCLUDE_PREFIX}")
endif()

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

add_library(PixelLinkDesktop STATIC
    src/PixelLink/Frontend/ROMLibrary.cpp
    src/PixelLink/Frontend/TextRenderer.cpp
    src/PixelLink/Frontend/Application.cpp
)
target_link_libraries(PixelLinkDesktop PUBLIC PixelLink::Emulator)
target_compile_definitions(PixelLinkDesktop PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(WIN32)
    target_link_libraries(PixelLinkDesktop PRIVATE gdi32 user32)
endif()

add_executable(PixelLink WIN32 src/PixelLink/Main.cpp)
target_link_libraries(PixelLink PRIVATE PixelLinkDesktop)
target_compile_definitions(PixelLink PRIVATE PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}")

# ------------------------------------------------------------
# Logging utilities
# ------------------------------------------------------------

add_library(PixelLinkUtils STATIC src/PixelLink/Utils/Logger.cpp)
add_library(PixelLink::Utils ALIAS PixelLinkUtils)
target_include_directories(PixelLinkUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(PixelLinkUtils PUBLIC spdlog::spdlog)

# ------------------------------------------------------------
# Game Boy tests
# ------------------------------------------------------------

enable_testing()
add_library(PixelLinkTestSupport STATIC
    "src/PixelLink/Test/TestRunner.cpp"
)
add_library(PixelLink::TestSupport ALIAS PixelLinkTestSupport)
target_include_directories(PixelLinkTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(PixelLinkTestSupport PUBLIC PixelLink::Utils)
target_compile_definitions(PixelLinkTestSupport PRIVATE
    PIXELLINK_TEST_LOG_DIR="${CMAKE_BINARY_DIR}/logs"
)

add_executable(GameBoyTest
    Test/PixelLink/GameBoy/Main.cpp
    Test/PixelLink/GameBoy/APUTest.cpp
    Test/PixelLink/GameBoy/APUAccuracyTest.cpp
    Test/PixelLink/GameBoy/BusTest.cpp
    Test/PixelLink/GameBoy/CartridgeTest.cpp
    Test/PixelLink/GameBoy/CPUTest.cpp
    Test/PixelLink/GameBoy/CoreTimingTest.cpp
    Test/PixelLink/GameBoy/HardwareAccuracyTest.cpp
    Test/PixelLink/GameBoy/InterruptTest.cpp
    Test/PixelLink/GameBoy/JoypadTest.cpp
    Test/PixelLink/GameBoy/MapperFactoryTest.cpp
    Test/PixelLink/GameBoy/MBC1Test.cpp
    Test/PixelLink/GameBoy/MBC3Test.cpp
    Test/PixelLink/GameBoy/MBC5Test.cpp
    Test/PixelLink/GameBoy/PPUTest.cpp
    Test/PixelLink/GameBoy/RTCTest.cpp
    Test/PixelLink/GameBoy/SaveTest.cpp
    Test/PixelLink/GameBoy/SerialTest.cpp
    Test/PixelLink/GameBoy/TimerIntegrationTest.cpp
    Test/PixelLink/GameBoy/TimerTest.cpp
)

target_link_libraries(GameBoyTest
    PRIVATE
        PixelLink::GameBoy
        PixelLink::TestSupport
)

target_compile_definitions(GameBoyTest
    PRIVATE
        PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
        PIXELLINK_APU_TEST_DIR="${CMAKE_BINARY_DIR}/APUTest"
)

add_test(
    NAME GameBoyTest
    COMMAND GameBoyTest
)

# The hardware suite runs all 40 ROMs inside GameBoyTest and reports every
# failure before returning. Preserve the previous host budget across the set.
set_tests_properties(GameBoyTest PROPERTIES LABELS accuracy TIMEOUT 4800)

# ------------------------------------------------------------
# Emulator integration tests
# ------------------------------------------------------------

add_executable(FrontendTest
    Test/PixelLink/Frontend/Main.cpp
    Test/PixelLink/Frontend/AudioTest.cpp
    Test/PixelLink/Frontend/EmulatorTest.cpp
)

target_link_libraries(FrontendTest
    PRIVATE
        PixelLink::Emulator
        PixelLink::TestSupport
)

target_compile_definitions(FrontendTest
    PRIVATE
        PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME FrontendTest
    COMMAND FrontendTest
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

pixellink_deploy_sdl_runtime(FrontendTest)
pixellink_deploy_sdl_runtime(PixelLink)

# ------------------------------------------------------------
# Desktop application tests
# ------------------------------------------------------------

add_executable(DesktopTest
    Test/PixelLink/Desktop/Main.cpp
    Test/PixelLink/Desktop/AudioTest.cpp
    Test/PixelLink/Desktop/ROMLibraryTest.cpp
    Test/PixelLink/Desktop/ApplicationTest.cpp
    Test/PixelLink/Desktop/LocalGameTest.cpp
    Test/PixelLink/Desktop/LibraryPreviewTest.cpp
    Test/PixelLink/Desktop/TestUtils.cpp
    # Reuse the automatic audio check also run by FrontendTest.
    Test/PixelLink/Frontend/AudioTest.cpp
)
target_link_libraries(DesktopTest PRIVATE PixelLinkDesktop PixelLink::TestSupport)
target_compile_definitions(DesktopTest PRIVATE PIXELLINK_DESKTOP_TEST_DIR="${CMAKE_BINARY_DIR}/DesktopTest")
target_compile_definitions(DesktopTest PRIVATE PIXELLINK_PROJECT_DIR="${CMAKE_SOURCE_DIR}")
pixellink_deploy_sdl_runtime(DesktopTest)
add_test(NAME DesktopTest COMMAND DesktopTest)
set_tests_properties(DesktopTest PROPERTIES TIMEOUT 90 LABELS "frontend;automatic")
# The existing FrontendTest intentionally opens a user-controlled game window.
set_tests_properties(FrontendTest PROPERTIES LABELS "frontend;interactive")
