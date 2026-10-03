add_executable(pwt_native_tests
    foundation_smoke.c
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pwt_native_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME foundation_smoke COMMAND pwt_native_tests)
