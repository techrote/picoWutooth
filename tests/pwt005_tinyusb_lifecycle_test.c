#include <stdio.h>
#include <stdlib.h>
#include "controller_stub.h"
#include "usb_hci_tinyusb.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static bool mounted;
static bool arm_succeeds;
static unsigned arm_calls;

bool tud_mounted(void) { return mounted; }
bool tud_bt_event_send(void *event, uint16_t length) {
    (void)event; (void)length; return mounted;
}
bool tud_bt_acl_data_send(void *data, uint16_t length) {
    (void)data; (void)length; return mounted;
}
bool tud_bt_acl_data_receive_ready(void) {
    ++arm_calls;
    return mounted && arm_succeeds;
}
void tud_umount_cb(void);

int main(void) {
    pwt_controller_stub_t stub;
    pwt_usb_hci_router_t router;
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_init(&router, pwt_controller_stub_backend(&stub),
                           pwt_usb_hci_tinyusb_transport());
    pwt_usb_hci_tinyusb_bind(&router);

    /* The real BTH class has no OUT endpoint before SET_CONFIGURATION. */
    for (unsigned i = 0; i < 100; ++i) {
        pwt_usb_hci_tinyusb_service_ingress();
        CHECK(!pwt_usb_hci_tinyusb_recovery_required());
    }
    CHECK(arm_calls == 0);

    mounted = true;
    arm_succeeds = true;
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(arm_calls == 1);
    CHECK(!pwt_usb_hci_tinyusb_recovery_required());

    /* Busy bounded ingress must continue to exert ordinary USB backpressure. */
    router.acl_complete_pending = true;
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(arm_calls == 1);
    router.acl_complete_pending = false;

    /* Configuration loss/replug must not create a premature endpoint fault. */
    mounted = false;
    tud_umount_cb();
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(arm_calls == 1);
    CHECK(!pwt_usb_hci_tinyusb_recovery_required());
    CHECK(stub.reset_count == 1);
    mounted = true;
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(arm_calls == 2);

    /* An actual arm failure after configuration remains a recovery fault. */
    arm_succeeds = false;
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(pwt_usb_hci_tinyusb_recovery_required());
    pwt_usb_hci_tinyusb_clear_recovery();
    arm_succeeds = true;
    pwt_usb_hci_tinyusb_service_ingress();
    CHECK(!pwt_usb_hci_tinyusb_recovery_required());
    puts("PWT-005 TinyUSB configuration lifecycle passed");
    return 0;
}
