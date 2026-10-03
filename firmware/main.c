#include "pico/stdlib.h"

#include "bsp/board_api.h"
#include "tusb.h"

#include "controller_stub.h"
#include "usb_hci_router.h"
#include "usb_hci_tinyusb.h"

#ifndef PWT_SOURCE_COMMIT
#define PWT_SOURCE_COMMIT "unknown"
#endif

static const char pwt_build_identity[] __attribute__((used)) =
    "picoWutooth-source=" PWT_SOURCE_COMMIT;

int main(void) {
    (void)pwt_build_identity;

    board_init();

    static pwt_controller_stub_t stub;
    static pwt_usb_hci_router_t router;
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_init(
        &router,
        pwt_controller_stub_backend(&stub),
        pwt_usb_hci_tinyusb_transport());
    pwt_usb_hci_tinyusb_bind(&router);

    if (!tusb_init()) {
        for (;;) {
            tight_loop_contents();
        }
    }

    if (board_init_after_tusb != NULL) {
        board_init_after_tusb();
    }

    for (;;) {
        tud_task();
        pwt_usb_hci_service(&router);
    }
}
