add_executable(pwt_foundation_smoke
    foundation_smoke.c
)

add_executable(pwt_usb_hci_tests
    pwt003_usb_hci_test.c
    ../firmware/controller_stub.c
    ../firmware/usb_descriptor_data.c
    ../firmware/usb_hci_router.c
)

target_include_directories(pwt_usb_hci_tests PRIVATE ../firmware)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_foundation_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(pwt_usb_hci_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME foundation_smoke COMMAND pwt_foundation_smoke)
add_test(NAME pwt003_usb_hci COMMAND pwt_usb_hci_tests)
