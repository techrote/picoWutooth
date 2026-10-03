#include "usb_hci_tinyusb.h"

#include <stddef.h>

#include "tusb.h"

static pwt_usb_hci_router_t *active_router;

static bool tinyusb_send_event(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return tud_bt_event_send((void *)data, length);
}

static bool tinyusb_send_acl(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return tud_bt_acl_data_send((void *)data, length);
}

void pwt_usb_hci_tinyusb_bind(pwt_usb_hci_router_t *router) {
    active_router = router;
}

pwt_usb_tx_t pwt_usb_hci_tinyusb_transport(void) {
    const pwt_usb_tx_t transport = {
        .context = NULL,
        .send_event = tinyusb_send_event,
        .send_acl = tinyusb_send_acl,
    };
    return transport;
}

void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len) {
    if (active_router == NULL || hci_cmd == NULL || cmd_len > UINT16_MAX) {
        return;
    }
    (void)pwt_usb_hci_receive_command(active_router, hci_cmd, (uint16_t)cmd_len);
}

void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len) {
    if (active_router == NULL || acl_data == NULL) {
        return;
    }
    (void)pwt_usb_hci_receive_acl(active_router, acl_data, data_len);
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
}
