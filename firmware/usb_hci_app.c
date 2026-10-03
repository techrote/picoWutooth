#include "usb_hci_app.h"

#include "bsp/board_api.h"
#include "tusb.h"

#include "controller_stub.h"
#include "usb_hci_router.h"
#include "usb_hci_tinyusb.h"

static pwt_controller_stub_t pwt_usb_stub;
static pwt_usb_hci_router_t pwt_usb_router;

bool pwt_usb_hci_app_init(void) {
    pwt_controller_stub_init(&pwt_usb_stub);
    pwt_usb_hci_router_init(
        &pwt_usb_router,
        pwt_controller_stub_backend(&pwt_usb_stub),
        pwt_usb_hci_tinyusb_transport());
    pwt_usb_hci_tinyusb_bind(&pwt_usb_router);

    if (!tusb_init()) {
        return false;
    }

    if (board_init_after_tusb != NULL) {
        board_init_after_tusb();
    }
    return true;
}

void pwt_usb_hci_app_task(void) {
    tud_task();
    pwt_usb_hci_service(&pwt_usb_router);
}
