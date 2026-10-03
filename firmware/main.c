#include "pico/stdlib.h"
#include "picowutooth/cyw43_hci_backend.h"
#include "picowutooth/hci_transport.h"

#ifndef PWT_SOURCE_COMMIT
#define PWT_SOURCE_COMMIT "unknown"
#endif

static const char pwt_build_identity[] __attribute__((used)) =
    "picoWutooth-source=" PWT_SOURCE_COMMIT;

static pwt_cyw43_hci_backend_t pwt_cyw43_backend;
static pwt_hci_transport_t pwt_hci_transport;

int main(void) {
    (void)pwt_build_identity;

    pwt_cyw43_hci_backend_init(&pwt_cyw43_backend);
    (void)pwt_hci_transport_init(
        &pwt_hci_transport, pwt_cyw43_hci_backend_ops(), &pwt_cyw43_backend);

    /* PWT-003/PWT-004 own USB and bridge integration. */
    for (;;) {
        tight_loop_contents();
    }
}
