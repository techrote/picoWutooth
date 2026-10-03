#include "usb_hci_tinyusb.h"

#include <stddef.h>

#include "tusb.h"

static pwt_usb_hci_router_t *active_router;
static bool callback_fault;

static bool tinyusb_send_event(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return tud_bt_event_send((void *)data, length);
}

static bool tinyusb_send_acl(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return tud_bt_acl_data_send((void *)data, length);
}

static void arm_acl_if_ready(void) {
    if (active_router != NULL && pwt_usb_hci_can_receive_acl(active_router)) {
        /*
         * PWT-004's revision-locked TinyUSB patch makes ACL OUT re-arming
         * explicit. If the endpoint is already armed this is a harmless no-op.
         */
        if (!tud_bt_acl_data_receive_ready()) {
            callback_fault = true;
        }
    }
}

void pwt_usb_hci_tinyusb_bind(pwt_usb_hci_router_t *router) {
    active_router = router;
    callback_fault = false;
}

pwt_usb_tx_t pwt_usb_hci_tinyusb_transport(void) {
    const pwt_usb_tx_t transport = {
        .context = NULL,
        .send_event = tinyusb_send_event,
        .send_acl = tinyusb_send_acl,
    };
    return transport;
}

void pwt_usb_hci_tinyusb_service_ingress(void) {
    arm_acl_if_ready();
}

bool pwt_usb_hci_tinyusb_recovery_required(void) {
    return callback_fault ||
           (active_router != NULL && pwt_usb_hci_recovery_required(active_router));
}

void pwt_usb_hci_tinyusb_clear_recovery(void) {
    callback_fault = false;
}

void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len) {
    if (active_router == NULL || hci_cmd == NULL || cmd_len > UINT16_MAX) {
        callback_fault = true;
        return;
    }

    if (pwt_usb_hci_receive_command(
            active_router, hci_cmd, (uint16_t)cmd_len) != PWT_BACKEND_ACCEPTED) {
        /*
         * EP0 cannot be retroactively backpressured after the class callback.
         * A command that cannot be safely copied is therefore an observable
         * transport fault and the app will force USB/controller recovery.
         */
        callback_fault = true;
    }
}

void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len) {
    if (active_router == NULL || acl_data == NULL || data_len == 0u) {
        callback_fault = true;
        return;
    }

    const pwt_backend_submit_result_t result =
        pwt_usb_hci_receive_acl(active_router, acl_data, data_len);
    if (result == PWT_BACKEND_REJECTED) {
        callback_fault = true;
        return;
    }

    /*
     * ACCEPTED means this fragment can be followed immediately. BUSY means a
     * complete HCI packet is retained in bounded storage; keep OUT unarmed so
     * the host receives NAKs until service_ingress() can safely resume it.
     */
    arm_acl_if_ready();
}

void tud_bt_event_sent_cb(uint16_t sent_bytes) {
    if (active_router != NULL) {
        pwt_usb_hci_event_sent(active_router, sent_bytes);
    }
}

void tud_bt_acl_data_sent_cb(uint16_t sent_bytes) {
    if (active_router != NULL) {
        pwt_usb_hci_acl_sent(active_router, sent_bytes);
    }
}

void tud_umount_cb(void) {
    if (active_router != NULL) {
        pwt_usb_hci_reset(active_router);
    }
    callback_fault = false;
}
