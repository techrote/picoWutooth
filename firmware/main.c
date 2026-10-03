#include "pico/stdlib.h"

#include "bsp/board_api.h"

#include "picowutooth/cyw43_hci_backend.h"
#include "picowutooth/hci_transport.h"
#include "usb_hci_app.h"

#ifndef PWT_SOURCE_COMMIT
#define PWT_SOURCE_COMMIT "unknown"
#endif

static const char pwt_build_identity[] __attribute__((used)) =
    "picoWutooth-source=" PWT_SOURCE_COMMIT;

static pwt_cyw43_hci_backend_t pwt_cyw43_backend;
static pwt_hci_transport_t pwt_hci_transport;

int main(void) {
    (void)pwt_build_identity;

    board_init();

    /*
     * PWT-002 and PWT-003 remain independent halves. PWT-004 owns the bridge;
     * this firmware deliberately does not route TinyUSB traffic to CYW43.
     */
    pwt_cyw43_hci_backend_init(&pwt_cyw43_backend);
    (void)pwt_hci_transport_init(
        &pwt_hci_transport, pwt_cyw43_hci_backend_ops(), &pwt_cyw43_backend);

    if (!pwt_usb_hci_app_init()) {
        for (;;) {
            tight_loop_contents();
        }
    }

    for (;;) {
        pwt_usb_hci_app_task();
        tight_loop_contents();
    }
}
