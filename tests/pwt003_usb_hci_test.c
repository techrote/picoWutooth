#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controller_stub.h"
#include "usb_descriptor_data.h"
#include "usb_hci_router.h"

#define USB_DESC_CONFIGURATION 0x02u
#define USB_DESC_INTERFACE 0x04u
#define USB_DESC_ENDPOINT 0x05u
#define USB_DESC_INTERFACE_ASSOCIATION 0x0bu
#define USB_CLASS_CDC 0x02u
#define USB_CLASS_HID 0x03u
#define USB_CLASS_VENDOR 0xffu
#define USB_XFER_CONTROL 0x00u
#define USB_XFER_ISOCHRONOUS 0x01u
#define USB_XFER_BULK 0x02u
#define USB_XFER_INTERRUPT 0x03u

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)

typedef struct {
    bool event_ready;
    bool acl_ready;
    uint8_t event_data[260];
    uint16_t event_length;
    uint8_t acl_data[260];
    uint16_t acl_length;
    uint32_t event_calls;
    uint32_t acl_calls;
} fake_usb_t;

static bool fake_send_event(void *context, const uint8_t *data, uint16_t length) {
    fake_usb_t *usb = context;
    usb->event_calls++;
    if (!usb->event_ready) {
        return false;
    }
    CHECK(length <= sizeof(usb->event_data));
    memcpy(usb->event_data, data, length);
    usb->event_length = length;
    return true;
}

static bool fake_send_acl(void *context, const uint8_t *data, uint16_t length) {
    fake_usb_t *usb = context;
    usb->acl_calls++;
    if (!usb->acl_ready) {
        return false;
    }
    CHECK(length <= sizeof(usb->acl_data));
    memcpy(usb->acl_data, data, length);
    usb->acl_length = length;
    return true;
}

static pwt_usb_hci_router_t make_router(pwt_controller_stub_t *stub, fake_usb_t *usb) {
    pwt_usb_hci_router_t router;
    const pwt_usb_tx_t tx = {
        .context = usb,
        .send_event = fake_send_event,
        .send_acl = fake_send_acl,
    };
    pwt_usb_hci_router_init(&router, pwt_controller_stub_backend(stub), tx);
    return router;
}

static uint16_t read_u16_le(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static void test_descriptor_contract(void) {
    const uint8_t *device = pwt_usb_device_descriptor;
    CHECK(device[0] == PWT_USB_DEVICE_DESCRIPTOR_LENGTH);
    CHECK(device[1] == 0x01u);
    CHECK(device[4] == PWT_USB_CLASS_WIRELESS_CONTROLLER);
    CHECK(device[5] == PWT_USB_BTH_SUBCLASS);
    CHECK(device[6] == PWT_USB_BTH_PROTOCOL_PRIMARY_CONTROLLER);
    CHECK(device[7] == 64u);
    CHECK(device[17] == 1u);

    const uint8_t *cfg = pwt_usb_configuration_descriptor;
    CHECK(cfg[0] == 9u);
    CHECK(cfg[1] == USB_DESC_CONFIGURATION);
    CHECK(read_u16_le(&cfg[2]) == PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH);
    CHECK(cfg[4] == 1u);

    size_t offset = cfg[0];
    uint32_t interface_count = 0;
    uint32_t iad_count = 0;
    uint32_t endpoint_count = 0;
    bool saw_event = false;
    bool saw_acl_in = false;
    bool saw_acl_out = false;

    while (offset < PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH) {
        const uint8_t length = cfg[offset];
        const uint8_t type = cfg[offset + 1u];
        CHECK(length >= 2u);
        CHECK(offset + length <= PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH);

        if (type == USB_DESC_INTERFACE_ASSOCIATION) {
            iad_count++;
        } else if (type == USB_DESC_INTERFACE) {
            CHECK(length == 9u);
            interface_count++;
            CHECK(cfg[offset + 2u] == 0u);
            CHECK(cfg[offset + 3u] == 0u);
            CHECK(cfg[offset + 4u] == 3u);
            CHECK(cfg[offset + 5u] == PWT_USB_CLASS_WIRELESS_CONTROLLER);
            CHECK(cfg[offset + 6u] == PWT_USB_BTH_SUBCLASS);
            CHECK(cfg[offset + 7u] == PWT_USB_BTH_PROTOCOL_PRIMARY_CONTROLLER);
            CHECK(cfg[offset + 5u] != USB_CLASS_CDC);
            CHECK(cfg[offset + 5u] != USB_CLASS_HID);
            CHECK(cfg[offset + 5u] != USB_CLASS_VENDOR);
        } else if (type == USB_DESC_ENDPOINT) {
            CHECK(length == 7u);
            endpoint_count++;
            const uint8_t address = cfg[offset + 2u];
            const uint8_t transfer_type = cfg[offset + 3u] & 0x03u;
            CHECK(transfer_type != USB_XFER_CONTROL);
            CHECK(transfer_type != USB_XFER_ISOCHRONOUS);

            if (address == PWT_USB_EP_EVENT_IN) {
                CHECK(transfer_type == USB_XFER_INTERRUPT);
                CHECK(read_u16_le(&cfg[offset + 4u]) == PWT_USB_EVENT_EP_SIZE);
                saw_event = true;
            } else if (address == PWT_USB_EP_ACL_IN) {
                CHECK(transfer_type == USB_XFER_BULK);
                CHECK(read_u16_le(&cfg[offset + 4u]) == PWT_USB_ACL_EP_SIZE);
                saw_acl_in = true;
            } else if (address == PWT_USB_EP_ACL_OUT) {
                CHECK(transfer_type == USB_XFER_BULK);
                CHECK(read_u16_le(&cfg[offset + 4u]) == PWT_USB_ACL_EP_SIZE);
                saw_acl_out = true;
            } else {
                CHECK(false && "unexpected production endpoint");
            }
        } else {
            CHECK(false && "unexpected descriptor type in production configuration");
        }

        offset += length;
    }

    CHECK(offset == PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH);
    CHECK(interface_count == 1u);
    CHECK(iad_count == 0u);
    CHECK(endpoint_count == 3u);
    CHECK(saw_event && saw_acl_in && saw_acl_out);
}

static void test_host_to_controller_routing(void) {
    pwt_controller_stub_t stub;
    fake_usb_t usb = {0};
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_t router = make_router(&stub, &usb);

    const uint8_t command[] = {0x03, 0x0c, 0x00};
    CHECK(pwt_usb_hci_receive_command(&router, command, sizeof(command)) == PWT_BACKEND_ACCEPTED);
    CHECK(stub.host_submit_count == 1u);
    CHECK(stub.last_host_kind == PWT_HCI_PACKET_COMMAND);
    CHECK(stub.last_host_length == sizeof(command));
    CHECK(memcmp(stub.last_host_data, command, sizeof(command)) == 0);

    const uint8_t acl[] = {0x01, 0x20, 0x02, 0x00, 0xaa, 0x55};
    CHECK(pwt_usb_hci_receive_acl(&router, acl, sizeof(acl)) == PWT_BACKEND_ACCEPTED);
    CHECK(stub.host_submit_count == 2u);
    CHECK(stub.last_host_kind == PWT_HCI_PACKET_ACL);
    CHECK(stub.last_host_length == sizeof(acl));
    CHECK(memcmp(stub.last_host_data, acl, sizeof(acl)) == 0);

    const pwt_usb_hci_stats_t *stats = pwt_usb_hci_stats(&router);
    CHECK(stats->host_commands_accepted == 1u);
    CHECK(stats->host_acl_accepted == 1u);
}

static void test_controller_event_ownership_and_busy_retry(void) {
    pwt_controller_stub_t stub;
    fake_usb_t usb = {.event_ready = false, .acl_ready = true};
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_t router = make_router(&stub, &usb);

    const uint8_t event[] = {0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
    CHECK(pwt_controller_stub_queue_to_host(&stub, PWT_HCI_PACKET_EVENT, event, sizeof(event)));

    pwt_usb_hci_service(&router);
    CHECK(usb.event_calls == 1u);
    CHECK(usb.acl_calls == 0u);
    CHECK(!stub.to_host_queued);
    CHECK(!stub.to_host_leased);
    CHECK(stub.release_count == 1u);

    usb.event_ready = true;
    pwt_usb_hci_service(&router);
    CHECK(usb.event_calls == 2u);
    CHECK(usb.event_length == sizeof(event));
    CHECK(memcmp(usb.event_data, event, sizeof(event)) == 0);
    CHECK(stub.release_count == 1u);

    pwt_usb_hci_event_sent(&router, sizeof(event));
    CHECK(stub.release_count == 1u);
    CHECK(!stub.to_host_queued);

    const pwt_usb_hci_stats_t *stats = pwt_usb_hci_stats(&router);
    CHECK(stats->usb_busy_retries == 1u);
    CHECK(stats->controller_packets_sent == 1u);
}

static void test_controller_acl_routes_only_to_bulk_in(void) {
    pwt_controller_stub_t stub;
    fake_usb_t usb = {.event_ready = true, .acl_ready = true};
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_t router = make_router(&stub, &usb);

    const uint8_t acl[] = {0x01, 0x20, 0x02, 0x00, 0xde, 0xad};
    CHECK(pwt_controller_stub_queue_to_host(&stub, PWT_HCI_PACKET_ACL, acl, sizeof(acl)));
    pwt_usb_hci_service(&router);
    CHECK(usb.event_calls == 0u);
    CHECK(usb.acl_calls == 1u);
    CHECK(memcmp(usb.acl_data, acl, sizeof(acl)) == 0);
    CHECK(stub.release_count == 1u);

    pwt_usb_hci_acl_sent(&router, sizeof(acl));
    CHECK(stub.release_count == 1u);
}

static void test_reset_releases_owned_packet(void) {
    pwt_controller_stub_t stub;
    fake_usb_t usb = {.event_ready = true};
    pwt_controller_stub_init(&stub);
    pwt_usb_hci_router_t router = make_router(&stub, &usb);

    const uint8_t event[] = {0x0f, 0x00};
    CHECK(pwt_controller_stub_queue_to_host(&stub, PWT_HCI_PACKET_EVENT, event, sizeof(event)));
    pwt_usb_hci_service(&router);
    CHECK(!stub.to_host_leased);
    CHECK(stub.release_count == 1u);

    pwt_usb_hci_reset(&router);
    CHECK(stub.release_count == 1u);
    CHECK(stub.reset_count == 1u);
    CHECK(!stub.to_host_queued);
    CHECK(pwt_usb_hci_stats(&router)->resets == 1u);
}

int main(void) {
    test_descriptor_contract();
    test_host_to_controller_routing();
    test_controller_event_ownership_and_busy_retry();
    test_controller_acl_routes_only_to_bulk_in();
    test_reset_releases_owned_packet();
    puts("PWT-003 descriptor and USB HCI routing tests passed");
    return 0;
}
