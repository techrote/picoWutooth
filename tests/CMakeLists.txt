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
