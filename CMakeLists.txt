cmake_minimum_required(VERSION 3.13)

option(PWT_BUILD_NATIVE_TESTS "Build host-native picoWutooth tests instead of firmware" OFF)

if(PWT_BUILD_NATIVE_TESTS)
    project(picowutooth_native_tests LANGUAGES C)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
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
    firmware/controller_stub.c
    firmware/usb_descriptor_data.c
    firmware/usb_descriptors.c
    firmware/usb_hci_router.c
    firmware/usb_hci_tinyusb.c
)

target_include_directories(picowutooth PRIVATE firmware)

target_link_libraries(picowutooth
    PRIVATE
        pico_stdlib
        tinyusb_device
        tinyusb_board
)

# Pico SDK 2.3.1's TinyUSB aggregate omits the BTH class source even though
# usbd.c registers it when CFG_TUD_BTH=1. Compile the exact pinned upstream
# source directly; tools/fetch-pico-sdk.* applies our zero-ISO compatibility
# patch to this same file before configuration.
target_sources(picowutooth PRIVATE
    "${PICO_TINYUSB_PATH}/src/class/bth/bth_device.c"
)

target_compile_definitions(picowutooth
    PRIVATE
        PWT_SOURCE_COMMIT="${PWT_SOURCE_COMMIT}"
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        firmware/main.c
        firmware/controller_stub.c
        firmware/usb_descriptor_data.c
        firmware/usb_descriptors.c
        firmware/usb_hci_router.c
        firmware/usb_hci_tinyusb.c
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
    )
endif()

pico_enable_stdio_usb(picowutooth 0)
pico_enable_stdio_uart(picowutooth 0)

pico_add_extra_outputs(picowutooth)
