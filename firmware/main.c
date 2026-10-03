#include "pico/stdlib.h"

#ifndef PWT_SOURCE_COMMIT
#define PWT_SOURCE_COMMIT "unknown"
#endif

/*
 * Keep the exact source identity in the linked image without exposing any USB
 * debug interface. CI also records this identity beside the UF2 in a manifest.
 */
static const char pwt_build_identity[] __attribute__((used)) =
    "picoWutooth-source=" PWT_SOURCE_COMMIT;

int main(void) {
    (void)pwt_build_identity;

    /*
     * PWT-001 deliberately does not initialize Bluetooth, CYW43, or USB HCI.
     * Later issues own those behaviours. This target only proves that the
     * pinned Pico W toolchain and project foundation build cleanly.
     */
    for (;;) {
        tight_loop_contents();
    }
}
