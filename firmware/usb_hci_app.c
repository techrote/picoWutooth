#include "usb_hci_app.h"

#include "bsp/board_api.h"
#include "pico/time.h"
#include "tusb.h"

#include "controller_bridge.h"
#include "usb_hci_router.h"
#include "usb_hci_tinyusb.h"

#define PWT_USB_RECOVERY_DISCONNECT_MS 20u
#define PWT_USB_RECOVERY_RETRY_MS 100u

static pwt_controller_bridge_t pwt_controller_bridge;
static pwt_usb_hci_router_t pwt_usb_router;

static bool recovery_disconnected;
static absolute_time_t recovery_reconnect_at;
static absolute_time_t recovery_retry_at;

static void begin_recovery(void) {
    if (!recovery_disconnected) {
        /*
         * Make transport loss observable to the host before resetting the
         * controller. The router's USB-owned egress buffer remains stable
         * across this sequence even if a TinyUSB completion arrives late.
         */
        (void)tud_disconnect();
        recovery_disconnected = true;
    }

    pwt_usb_hci_reset(&pwt_usb_router);
    pwt_usb_hci_tinyusb_clear_recovery();
    recovery_reconnect_at = make_timeout_time_ms(PWT_USB_RECOVERY_DISCONNECT_MS);
    recovery_retry_at = make_timeout_time_ms(PWT_USB_RECOVERY_RETRY_MS);
}

bool pwt_usb_hci_app_init(pwt_hci_transport_t *transport) {
    if (transport == NULL ||
        pwt_hci_transport_state(transport) != PWT_HCI_STATE_READY) {
        return false;
    }

    pwt_controller_bridge_init(&pwt_controller_bridge, transport);
    if (pwt_controller_bridge_needs_recovery(&pwt_controller_bridge)) {
        return false;
    }

    pwt_usb_hci_router_init(
        &pwt_usb_router,
        pwt_controller_bridge_backend(&pwt_controller_bridge),
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

    if (recovery_disconnected) {
        if (pwt_controller_bridge_needs_recovery(&pwt_controller_bridge)) {
            if (time_reached(recovery_retry_at)) {
                pwt_usb_hci_reset(&pwt_usb_router);
                pwt_usb_hci_tinyusb_clear_recovery();
                recovery_retry_at = make_timeout_time_ms(PWT_USB_RECOVERY_RETRY_MS);
            }
            return;
        }

        if (time_reached(recovery_reconnect_at)) {
            (void)tud_connect();
            recovery_disconnected = false;
            pwt_usb_hci_tinyusb_service_ingress();
        }
        return;
    }

    pwt_controller_bridge_service(&pwt_controller_bridge);
    pwt_usb_hci_service(&pwt_usb_router);
    pwt_usb_hci_tinyusb_service_ingress();

    if (pwt_controller_bridge_needs_recovery(&pwt_controller_bridge) ||
        pwt_usb_hci_tinyusb_recovery_required()) {
        begin_recovery();
    }
}
