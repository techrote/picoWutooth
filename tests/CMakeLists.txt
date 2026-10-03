add_executable(pwt_foundation_smoke
    foundation_smoke.c
)

add_executable(pwt_hci_transport_tests
    hci_transport_test.c
    ${CMAKE_SOURCE_DIR}/src/hci_transport.c
)

target_include_directories(pwt_hci_transport_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_foundation_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(pwt_hci_transport_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME foundation_smoke COMMAND pwt_foundation_smoke)
add_test(NAME hci_transport COMMAND pwt_hci_transport_tests)

add_executable(pwt_usb_hci_tests
    pwt003_usb_hci_test.c
    ${CMAKE_SOURCE_DIR}/firmware/controller_stub.c
    ${CMAKE_SOURCE_DIR}/firmware/usb_descriptor_data.c
    ${CMAKE_SOURCE_DIR}/firmware/usb_hci_router.c
)

target_include_directories(pwt_usb_hci_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/firmware
        ${CMAKE_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_usb_hci_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME pwt003_usb_hci COMMAND pwt_usb_hci_tests)

add_executable(pwt_bridge_tests
    pwt004_bridge_test.c
    ${CMAKE_SOURCE_DIR}/firmware/controller_bridge.c
    ${CMAKE_SOURCE_DIR}/firmware/usb_hci_router.c
    ${CMAKE_SOURCE_DIR}/src/hci_transport.c
)

target_include_directories(pwt_bridge_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/firmware
        ${CMAKE_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_bridge_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME pwt004_bridge COMMAND pwt_bridge_tests)

add_executable(pwt_tinyusb_lifecycle_tests
    pwt005_tinyusb_lifecycle_test.c
    ${CMAKE_SOURCE_DIR}/firmware/controller_stub.c
    ${CMAKE_SOURCE_DIR}/firmware/usb_hci_router.c
    ${CMAKE_SOURCE_DIR}/firmware/usb_hci_tinyusb.c
)
target_include_directories(pwt_tinyusb_lifecycle_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
    ${CMAKE_SOURCE_DIR}/firmware
    ${CMAKE_SOURCE_DIR}/include
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_tinyusb_lifecycle_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME pwt005_tinyusb_lifecycle COMMAND pwt_tinyusb_lifecycle_tests)
