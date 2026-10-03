cmake_minimum_required(VERSION 3.13)

option(PWT_BUILD_NATIVE_TESTS "Build host-native picoWutooth tests instead of firmware" OFF)

if(PWT_BUILD_NATIVE_TESTS)
    project(picowutooth_native_tests LANGUAGES C)

    enable_testing()
    add_subdirectory(tests)
    return()
endif()

set(PICO_BOARD "pico_w" CACHE STRING "Raspberry Pi Pico SDK board target")
set(PWT_SOURCE_COMMIT "unknown" CACHE STRING "Source commit embedded in the firmware build identity")

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/.deps/pico-sdk")
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR
        "Pico SDK not found at '${PICO_SDK_PATH}'. "
        "Run tools/fetch-pico-sdk.sh (or .ps1), or pass -DPICO_SDK_PATH=<path>."
    )
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(picowutooth LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(picowutooth
    firmware/main.c
)

target_link_libraries(picowutooth
    PRIVATE
        pico_stdlib
)

target_compile_definitions(picowutooth
    PRIVATE
        PWT_SOURCE_COMMIT="${PWT_SOURCE_COMMIT}"
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Pico SDK sources are compiled as part of the final target, so target-wide
    # warning flags would also police third-party code. Keep the strict gate on
    # picoWutooth-owned sources only.
    set_source_files_properties(
        firmware/main.c
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
    )
endif()

# The MVP production firmware must not accidentally grow a CDC/USB stdio
# interface. PWT-003 owns the eventual single-purpose Bluetooth HCI USB class.
pico_enable_stdio_usb(picowutooth 0)
pico_enable_stdio_uart(picowutooth 0)

pico_add_extra_outputs(picowutooth)
