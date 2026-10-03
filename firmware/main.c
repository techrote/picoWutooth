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

static void halt_without_usb(void) {
    for (;;) {
        tight_loop_contents();
    }
}

int main(void) {
    (void)pwt_build_identity;

    board_init();

    /*
     * The controller must complete firmware/bootstrap and reach READY before
     * the host can enumerate the Bluetooth HCI USB function.
     */
    pwt_cyw43_hci_backend_init(&pwt_cyw43_backend);
    if (pwt_hci_transport_init(
            &pwt_hci_transport,
            pwt_cyw43_hci_backend_ops(),
            &pwt_cyw43_backend) != PWT_HCI_OK) {
        halt_without_usb();
    }

    if (!pwt_usb_hci_app_init(&pwt_hci_transport)) {
        pwt_hci_transport_deinit(&pwt_hci_transport);
        halt_without_usb();
    }

    for (;;) {
        pwt_usb_hci_app_task();
        tight_loop_contents();
    }
}
