#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controller_bridge.h"
#include "usb_hci_router.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)

#define FAKE_RX_CAPACITY 16u
#define FAKE_SENT_CAPACITY 32u

typedef struct {
    pwt_hci_packet_t rx[FAKE_RX_CAPACITY];
    uint8_t rx_head;
    uint8_t rx_tail;
    uint8_t rx_count;

    pwt_hci_packet_t sent[FAKE_SENT_CAPACITY];
    uint8_t sent_count;

    pwt_hci_status_t send_status;
    pwt_hci_status_t receive_status;
    pwt_hci_status_t reset_status;
    uint32_t reset_count;
} fake_link_t;

typedef struct {
    bool event_ready;
    bool acl_ready;
    uint8_t event_data[PWT_HCI_MAX_PAYLOAD];
    uint16_t event_length;
    uint8_t acl_data[PWT_HCI_MAX_PAYLOAD];
    uint16_t acl_length;
    uint32_t event_calls;
    uint32_t acl_calls;
} fake_usb_t;

static pwt_hci_status_t fake_link_send(void *context, const pwt_hci_packet_t *packet) {
    fake_link_t *link = context;
    if (link == NULL || packet == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (link->send_status != PWT_HCI_OK) {
        return link->send_status;
    }
    CHECK(link->sent_count < FAKE_SENT_CAPACITY);
    link->sent[link->sent_count++] = *packet;
    return PWT_HCI_OK;
}

static pwt_hci_status_t fake_link_receive(void *context, pwt_hci_packet_t *packet) {
    fake_link_t *link = context;
    if (link == NULL || packet == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (link->receive_status != PWT_HCI_OK) {
        return link->receive_status;
    }
    if (link->rx_count == 0u) {
        return PWT_HCI_NO_DATA;
    }

    *packet = link->rx[link->rx_head];
    link->rx_head = (uint8_t)((link->rx_head + 1u) % FAKE_RX_CAPACITY);
    link->rx_count--;
    return PWT_HCI_OK;
}

static pwt_hci_status_t fake_link_reset(void *context) {
    fake_link_t *link = context;
    if (link == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    link->reset_count++;
    return link->reset_status;
}

static const pwt_hci_link_ops_t fake_link_ops = {
    .send = fake_link_send,
    .receive = fake_link_receive,
    .reset = fake_link_reset,
};

static void fake_link_init(fake_link_t *link) {
    memset(link, 0, sizeof(*link));
    link->send_status = PWT_HCI_OK;
    link->receive_status = PWT_HCI_OK;
    link->reset_status = PWT_HCI_OK;
}

static void queue_controller_packet(
    fake_link_t *link,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length) {
    CHECK(link != NULL);
    CHECK(data != NULL);
    CHECK(length <= PWT_HCI_MAX_PAYLOAD);
    CHECK(link->rx_count < FAKE_RX_CAPACITY);

    pwt_hci_packet_t *packet = &link->rx[link->rx_tail];
    memset(packet, 0, sizeof(*packet));
    packet->kind = kind;
    packet->length = length;
    memcpy(packet->payload, data, length);

    link->rx_tail = (uint8_t)((link->rx_tail + 1u) % FAKE_RX_CAPACITY);
    link->rx_count++;
}

static void queue_command_complete(fake_link_t *link, uint16_t opcode) {
    const uint8_t event[] = {
        0x0e, 0x04, 0x01,
        (uint8_t)(opcode & 0xffu),
        (uint8_t)(opcode >> 8u),
        0x00
    };
    queue_controller_packet(link, PWT_HCI_PACKET_EVENT, event, sizeof(event));
}

static void queue_acl_complete(fake_link_t *link, uint16_t handle) {
    const uint8_t event[] = {
        0x13, 0x05, 0x01,
        (uint8_t)(handle & 0xffu),
        (uint8_t)(handle >> 8u),
        0x01, 0x00
    };
    queue_controller_packet(link, PWT_HCI_PACKET_EVENT, event, sizeof(event));
}

static uint16_t make_acl(
    uint8_t *packet,
    uint16_t handle,
    uint16_t payload_length,
    uint8_t seed) {
    CHECK((uint32_t)payload_length + 4u <= PWT_HCI_MAX_PAYLOAD);
    packet[0] = (uint8_t)(handle & 0xffu);
    packet[1] = (uint8_t)((handle >> 8u) & 0x0fu);
    packet[2] = (uint8_t)(payload_length & 0xffu);
    packet[3] = (uint8_t)(payload_length >> 8u);
    for (uint16_t i = 0u; i < payload_length; ++i) {
        packet[4u + i] = (uint8_t)(seed + i);
    }
    return (uint16_t)(payload_length + 4u);
}

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

static void make_stack(
    fake_link_t *link,
    fake_usb_t *usb,
    pwt_controller_bridge_t *bridge,
    pwt_usb_hci_router_t *router) {
    pwt_controller_bridge_init_with_link(bridge, &fake_link_ops, link);
    const pwt_usb_tx_t tx = {
        .context = usb,
        .send_event = fake_send_event,
        .send_acl = fake_send_acl,
    };
    pwt_usb_hci_router_init(
        router,
        pwt_controller_bridge_backend(bridge),
        tx);
}

static void complete_usb_event(pwt_usb_hci_router_t *router, fake_usb_t *usb) {
    CHECK(usb->event_length > 0u);
    pwt_usb_hci_event_sent(router, usb->event_length);
    usb->event_length = 0u;
}

static void test_four_directions_and_acl_reassembly(void) {
    fake_link_t link;
    fake_usb_t usb = {.event_ready = true, .acl_ready = true};
    pwt_controller_bridge_t bridge;
    pwt_usb_hci_router_t router;
    fake_link_init(&link);
    make_stack(&link, &usb, &bridge, &router);

    const uint8_t reset_command[] = {0x03, 0x0c, 0x00};
    CHECK(pwt_usb_hci_receive_command(
              &router, reset_command, sizeof(reset_command)) == PWT_BACKEND_ACCEPTED);
    pwt_controller_bridge_service(&bridge);
    CHECK(link.sent_count == 1u);
    CHECK(link.sent[0].kind == PWT_HCI_PACKET_COMMAND);
    CHECK(link.sent[0].length == sizeof(reset_command));
    CHECK(memcmp(link.sent[0].payload, reset_command, sizeof(reset_command)) == 0);

    queue_command_complete(&link, 0x0c03u);
    pwt_controller_bridge_service(&bridge);
    pwt_usb_hci_service(&router);
    CHECK(usb.event_calls == 1u);
    CHECK(usb.event_data[0] == 0x0eu);
    complete_usb_event(&router, &usb);

    uint8_t acl[104];
    const uint16_t acl_length = make_acl(acl, 0x0001u, 100u, 0x20u);
    CHECK(acl_length == sizeof(acl));
    CHECK(pwt_usb_hci_receive_acl(&router, acl, 64u) == PWT_BACKEND_ACCEPTED);
    CHECK(pwt_usb_hci_can_receive_acl(&router));
    CHECK(pwt_usb_hci_receive_acl(
              &router, &acl[64], (uint16_t)(acl_length - 64u)) == PWT_BACKEND_ACCEPTED);

    pwt_controller_bridge_service(&bridge);
    CHECK(link.sent_count == 2u);
    CHECK(link.sent[1].kind == PWT_HCI_PACKET_ACL);
    CHECK(link.sent[1].length == acl_length);
    CHECK(memcmp(link.sent[1].payload, acl, acl_length) == 0);

    queue_acl_complete(&link, 0x0001u);
    pwt_controller_bridge_service(&bridge);
    pwt_usb_hci_service(&router);
    CHECK(usb.event_data[0] == 0x13u);
    complete_usb_event(&router, &usb);

    const uint8_t controller_acl[] = {0x01, 0x00, 0x02, 0x00, 0xde, 0xad};
    queue_controller_packet(
        &link, PWT_HCI_PACKET_ACL, controller_acl, sizeof(controller_acl));
    pwt_controller_bridge_service(&bridge);
    pwt_usb_hci_service(&router);
    CHECK(usb.acl_calls == 1u);
    CHECK(usb.acl_length == sizeof(controller_acl));
    CHECK(memcmp(usb.acl_data, controller_acl, sizeof(controller_acl)) == 0);
    pwt_usb_hci_acl_sent(&router, sizeof(controller_acl));

    CHECK(pwt_usb_hci_stats(&router)->host_commands_accepted == 1u);
    CHECK(pwt_usb_hci_stats(&router)->host_acl_fragments == 2u);
    CHECK(pwt_usb_hci_stats(&router)->host_acl_accepted == 1u);
    CHECK(pwt_controller_bridge_stats(&bridge)->controller_tx_sent == 2u);
}

static void test_acl_queue_full_backpressure_and_wraparound(void) {
    fake_link_t link;
    fake_usb_t usb = {.event_ready = true, .acl_ready = true};
    pwt_controller_bridge_t bridge;
    pwt_usb_hci_router_t router;
    fake_link_init(&link);
    make_stack(&link, &usb, &bridge, &router);

    uint8_t packets[5][5];
    for (uint16_t i = 0u; i < 5u; ++i) {
        CHECK(make_acl(packets[i], (uint16_t)(i + 1u), 1u, (uint8_t)i) == 5u);
    }

    for (uint16_t i = 0u; i < PWT_BRIDGE_ACL_QUEUE_CAPACITY; ++i) {
        CHECK(pwt_usb_hci_receive_acl(
                  &router, packets[i], sizeof(packets[i])) == PWT_BACKEND_ACCEPTED);
    }

    CHECK(pwt_usb_hci_receive_acl(
              &router, packets[4], sizeof(packets[4])) == PWT_BACKEND_BUSY);
    CHECK(!pwt_usb_hci_can_receive_acl(&router));

    pwt_controller_bridge_service(&bridge);
    CHECK(link.sent_count == 1u);
    CHECK((link.sent[0].payload[0] & 0x0fu) == 1u);

    /* One bridge slot is free; retrying the retained fifth packet wraps tail. */
    pwt_usb_hci_service(&router);
    CHECK(pwt_usb_hci_can_receive_acl(&router));

    for (uint16_t completed_handle = 1u; completed_handle <= 4u; ++completed_handle) {
        queue_acl_complete(&link, completed_handle);
        pwt_controller_bridge_service(&bridge);
        pwt_usb_hci_service(&router);
        CHECK(usb.event_length > 0u);
        complete_usb_event(&router, &usb);
    }

    CHECK(link.sent_count == 5u);
    for (uint16_t i = 0u; i < 5u; ++i) {
        const uint16_t handle =
            (uint16_t)(((uint16_t)link.sent[i].payload[0] |
                        ((uint16_t)link.sent[i].payload[1] << 8u)) &
                       0x0fffu);
        CHECK(handle == i + 1u);
    }

    queue_acl_complete(&link, 5u);
    pwt_controller_bridge_service(&bridge);
    pwt_usb_hci_service(&router);
    complete_usb_event(&router, &usb);

    CHECK(pwt_usb_hci_stats(&router)->host_acl_buffered == 1u);
    CHECK(pwt_controller_bridge_stats(&bridge)->host_queue_busy == 0u);
    CHECK(!pwt_controller_bridge_needs_recovery(&bridge));
}

static void test_usb_busy_retry_and_reset_ownership(void) {
    fake_link_t link;
    fake_usb_t usb = {.event_ready = false, .acl_ready = true};
    pwt_controller_bridge_t bridge;
    pwt_usb_hci_router_t router;
    fake_link_init(&link);
    make_stack(&link, &usb, &bridge, &router);

    const uint8_t event[] = {0x0f, 0x04, 0x00, 0x01, 0x01, 0x10};
    queue_controller_packet(&link, PWT_HCI_PACKET_EVENT, event, sizeof(event));
    pwt_controller_bridge_service(&bridge);
    pwt_usb_hci_service(&router);

    CHECK(usb.event_calls == 1u);
    CHECK(pwt_usb_hci_stats(&router)->usb_busy_retries == 1u);
    CHECK(bridge.rx_count == 0u);
    CHECK(!bridge.rx_leased);
    CHECK(pwt_controller_bridge_stats(&bridge)->release_mismatch == 0u);

    usb.event_ready = true;
    pwt_usb_hci_service(&router);
    CHECK(usb.event_calls == 2u);
    CHECK(memcmp(router.pending_to_usb, event, sizeof(event)) == 0);

    uint8_t stable_copy[sizeof(event)];
    memcpy(stable_copy, router.pending_to_usb, sizeof(stable_copy));
    pwt_usb_hci_reset(&router);

    CHECK(link.reset_count == 1u);
    CHECK(memcmp(router.pending_to_usb, stable_copy, sizeof(stable_copy)) == 0);
    CHECK(pwt_controller_bridge_stats(&bridge)->release_mismatch == 0u);
    CHECK(pwt_usb_hci_stats(&router)->packets_discarded_on_reset == 1u);
}

static void test_malformed_and_recovery_paths(void) {
    fake_link_t link;
    fake_usb_t usb = {.event_ready = true, .acl_ready = true};
    pwt_controller_bridge_t bridge;
    pwt_usb_hci_router_t router;
    fake_link_init(&link);
    make_stack(&link, &usb, &bridge, &router);

    const uint8_t oversized_acl_header[] = {0x01, 0x00, 0xff, 0x7f};
    CHECK(pwt_usb_hci_receive_acl(
              &router,
              oversized_acl_header,
              sizeof(oversized_acl_header)) == PWT_BACKEND_REJECTED);
    CHECK(pwt_usb_hci_recovery_required(&router));
    pwt_usb_hci_reset(&router);
    CHECK(!pwt_usb_hci_recovery_required(&router));
    CHECK(!pwt_controller_bridge_needs_recovery(&bridge));

    const uint8_t malformed_event[] = {0x0e, 0x04, 0x01};
    queue_controller_packet(
        &link, PWT_HCI_PACKET_EVENT, malformed_event, sizeof(malformed_event));
    pwt_controller_bridge_service(&bridge);
    CHECK(pwt_controller_bridge_needs_recovery(&bridge));
    CHECK(pwt_controller_bridge_stats(&bridge)->malformed_controller_packets == 1u);

    CHECK(pwt_controller_bridge_reset(&bridge) == PWT_HCI_OK);
    CHECK(!pwt_controller_bridge_needs_recovery(&bridge));

    const uint8_t command[] = {0x01, 0x10, 0x00};
    CHECK(pwt_usb_hci_receive_command(
              &router, command, sizeof(command)) == PWT_BACKEND_ACCEPTED);
    link.send_status = PWT_HCI_E_TRANSPORT;
    pwt_controller_bridge_service(&bridge);
    CHECK(pwt_controller_bridge_needs_recovery(&bridge));

    link.send_status = PWT_HCI_OK;
    CHECK(pwt_controller_bridge_reset(&bridge) == PWT_HCI_OK);
    CHECK(!pwt_controller_bridge_needs_recovery(&bridge));

    CHECK(pwt_usb_hci_receive_command(
              &router, command, sizeof(command)) == PWT_BACKEND_ACCEPTED);
    pwt_controller_bridge_service(&bridge);
    CHECK(link.sent_count == 1u);
    CHECK(link.sent[0].kind == PWT_HCI_PACKET_COMMAND);
}

int main(void) {
    test_four_directions_and_acl_reassembly();
    test_acl_queue_full_backpressure_and_wraparound();
    test_usb_busy_retry_and_reset_ownership();
    test_malformed_and_recovery_paths();
    puts("PWT-004 bridge/backpressure/recovery tests passed");
    return 0;
}
