#include "controller_bridge.h"

#include <stddef.h>
#include <string.h>

#define PWT_HCI_EVENT_COMMAND_COMPLETE 0x0eu
#define PWT_HCI_EVENT_COMMAND_STATUS 0x0fu
#define PWT_HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS 0x13u

static pwt_hci_status_t transport_send(void *context, const pwt_hci_packet_t *packet) {
    return pwt_hci_transport_send((pwt_hci_transport_t *)context, packet);
}

static pwt_hci_status_t transport_receive(void *context, pwt_hci_packet_t *packet) {
    return pwt_hci_transport_receive((pwt_hci_transport_t *)context, packet);
}

static pwt_hci_status_t transport_reset(void *context) {
    return pwt_hci_transport_reset((pwt_hci_transport_t *)context);
}

static const pwt_hci_link_ops_t production_link_ops = {
    .send = transport_send,
    .receive = transport_receive,
    .reset = transport_reset,
};

static bool link_valid(const pwt_hci_link_ops_t *ops) {
    return ops != NULL && ops->send != NULL && ops->receive != NULL && ops->reset != NULL;
}

static bool command_valid(const uint8_t *data, uint16_t length) {
    return data != NULL && length >= 3u &&
           (uint16_t)((uint16_t)data[2] + 3u) == length;
}

static bool acl_valid(const uint8_t *data, uint16_t length) {
    if (data == NULL || length < 4u) {
        return false;
    }
    const uint16_t payload_length =
        (uint16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8u));
    return (uint32_t)payload_length + 4u == length;
}

static bool controller_packet_valid(const pwt_hci_packet_t *packet) {
    if (packet == NULL || packet->length == 0u || packet->length > PWT_HCI_MAX_PAYLOAD) {
        return false;
    }
    if (packet->kind == PWT_HCI_PACKET_EVENT) {
        return packet->length >= 2u &&
               (uint16_t)((uint16_t)packet->payload[1] + 2u) == packet->length;
    }
    if (packet->kind == PWT_HCI_PACKET_ACL) {
        return acl_valid(packet->payload, packet->length);
    }
    return false;
}

static void clear_queues(pwt_controller_bridge_t *bridge) {
    memset(bridge->command_queue, 0, sizeof(bridge->command_queue));
    memset(bridge->acl_queue, 0, sizeof(bridge->acl_queue));
    memset(bridge->rx_queue, 0, sizeof(bridge->rx_queue));
    bridge->command_head = 0u;
    bridge->command_tail = 0u;
    bridge->command_count = 0u;
    bridge->acl_head = 0u;
    bridge->acl_tail = 0u;
    bridge->acl_count = 0u;
    bridge->rx_head = 0u;
    bridge->rx_tail = 0u;
    bridge->rx_count = 0u;
    bridge->rx_leased = false;
    bridge->wait_kind = PWT_BRIDGE_WAIT_NONE;
    bridge->wait_identifier = 0u;
}

static pwt_backend_submit_result_t bridge_submit_from_host(
    void *context,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length) {
    pwt_controller_bridge_t *bridge = context;
    if (bridge == NULL || bridge->faulted || length == 0u ||
        length > PWT_HCI_MAX_PAYLOAD) {
        return PWT_BACKEND_REJECTED;
    }

    pwt_bridge_slot_t *slot = NULL;
    uint8_t *tail = NULL;
    uint8_t *count = NULL;
    uint8_t capacity = 0u;

    if (kind == PWT_HCI_PACKET_COMMAND) {
        if (!command_valid(data, length)) {
            bridge->stats.malformed_host_packets++;
            return PWT_BACKEND_REJECTED;
        }
        tail = &bridge->command_tail;
        count = &bridge->command_count;
        capacity = PWT_BRIDGE_COMMAND_QUEUE_CAPACITY;
        if (*count < capacity) {
            slot = &bridge->command_queue[*tail];
        }
    } else if (kind == PWT_HCI_PACKET_ACL) {
        if (!acl_valid(data, length)) {
            bridge->stats.malformed_host_packets++;
            return PWT_BACKEND_REJECTED;
        }
        tail = &bridge->acl_tail;
        count = &bridge->acl_count;
        capacity = PWT_BRIDGE_ACL_QUEUE_CAPACITY;
        if (*count < capacity) {
            slot = &bridge->acl_queue[*tail];
        }
    } else {
        bridge->stats.malformed_host_packets++;
        return PWT_BACKEND_REJECTED;
    }

    if (slot == NULL) {
        bridge->stats.host_queue_busy++;
        return PWT_BACKEND_BUSY;
    }

    slot->packet.kind = kind;
    slot->packet.length = length;
    memcpy(slot->packet.payload, data, length);
    *tail = (uint8_t)((*tail + 1u) % capacity);
    (*count)++;

    if (kind == PWT_HCI_PACKET_COMMAND) {
        bridge->stats.host_commands_enqueued++;
    } else {
        bridge->stats.host_acl_enqueued++;
    }
    return PWT_BACKEND_ACCEPTED;
}

static bool bridge_can_accept_from_host(void *context, pwt_hci_packet_kind_t kind) {
    const pwt_controller_bridge_t *bridge = context;
    if (bridge == NULL || bridge->faulted) {
        return false;
    }
    if (kind == PWT_HCI_PACKET_COMMAND) {
        return bridge->command_count < PWT_BRIDGE_COMMAND_QUEUE_CAPACITY;
    }
    if (kind == PWT_HCI_PACKET_ACL) {
        return bridge->acl_count < PWT_BRIDGE_ACL_QUEUE_CAPACITY;
    }
    return false;
}

static bool bridge_peek_to_host(void *context, pwt_hci_packet_view_t *view) {
    pwt_controller_bridge_t *bridge = context;
    if (bridge == NULL || view == NULL || bridge->rx_count == 0u || bridge->faulted) {
        return false;
    }

    pwt_bridge_slot_t *slot = &bridge->rx_queue[bridge->rx_head];
    bridge->rx_leased = true;
    view->kind = slot->packet.kind;
    view->data = slot->packet.payload;
    view->length = slot->packet.length;
    view->token = slot;
    return true;
}

static void bridge_release_to_host(void *context, const void *token) {
    pwt_controller_bridge_t *bridge = context;
    if (bridge == NULL || bridge->rx_count == 0u || !bridge->rx_leased) {
        if (bridge != NULL) {
            bridge->stats.release_mismatch++;
            bridge->faulted = true;
        }
        return;
    }

    pwt_bridge_slot_t *slot = &bridge->rx_queue[bridge->rx_head];
    if (token != slot) {
        bridge->stats.release_mismatch++;
        bridge->faulted = true;
        return;
    }

    memset(slot, 0, sizeof(*slot));
    bridge->rx_head =
        (uint8_t)((bridge->rx_head + 1u) % PWT_BRIDGE_RX_QUEUE_CAPACITY);
    bridge->rx_count--;
    bridge->rx_leased = false;
}

static void bridge_backend_reset(void *context) {
    (void)pwt_controller_bridge_reset((pwt_controller_bridge_t *)context);
}

static void process_flow_event(
    pwt_controller_bridge_t *bridge,
    const pwt_hci_packet_t *packet) {
    if (bridge->wait_kind == PWT_BRIDGE_WAIT_NONE ||
        packet->kind != PWT_HCI_PACKET_EVENT || packet->length < 2u) {
        return;
    }

    const uint8_t *event = packet->payload;
    if (event[0] == PWT_HCI_EVENT_COMMAND_COMPLETE &&
        bridge->wait_kind == PWT_BRIDGE_WAIT_COMMAND &&
        packet->length >= 6u && event[1] >= 4u) {
        const uint16_t opcode =
            (uint16_t)((uint16_t)event[3] | ((uint16_t)event[4] << 8u));
        if (event[2] > 0u && opcode == bridge->wait_identifier) {
            bridge->wait_kind = PWT_BRIDGE_WAIT_NONE;
            bridge->stats.flow_wait_cleared++;
        }
        return;
    }

    if (event[0] == PWT_HCI_EVENT_COMMAND_STATUS &&
        bridge->wait_kind == PWT_BRIDGE_WAIT_COMMAND &&
        packet->length >= 6u && event[1] >= 4u) {
        const uint16_t opcode =
            (uint16_t)((uint16_t)event[4] | ((uint16_t)event[5] << 8u));
        if (event[3] > 0u && opcode == bridge->wait_identifier) {
            bridge->wait_kind = PWT_BRIDGE_WAIT_NONE;
            bridge->stats.flow_wait_cleared++;
        }
        return;
    }

    if (event[0] == PWT_HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS &&
        bridge->wait_kind == PWT_BRIDGE_WAIT_ACL && packet->length >= 3u) {
        const uint8_t handles = event[2];
        if ((uint32_t)3u + ((uint32_t)handles * 4u) != packet->length) {
            return;
        }

        for (uint8_t i = 0u; i < handles; ++i) {
            const size_t offset = 3u + ((size_t)i * 4u);
            const uint16_t handle =
                (uint16_t)(((uint16_t)event[offset] |
                            ((uint16_t)event[offset + 1u] << 8u)) &
                           0x0fffu);
            const uint16_t completed =
                (uint16_t)((uint16_t)event[offset + 2u] |
                           ((uint16_t)event[offset + 3u] << 8u));
            if (handle == bridge->wait_identifier && completed > 0u) {
                bridge->wait_kind = PWT_BRIDGE_WAIT_NONE;
                bridge->stats.flow_wait_cleared++;
                return;
            }
        }
    }
}

static void service_receive(pwt_controller_bridge_t *bridge) {
    if (bridge->rx_count >= PWT_BRIDGE_RX_QUEUE_CAPACITY) {
        return;
    }

    pwt_bridge_slot_t *slot = &bridge->rx_queue[bridge->rx_tail];
    memset(slot, 0, sizeof(*slot));
    const pwt_hci_status_t status =
        bridge->link_ops->receive(bridge->link_context, &slot->packet);
    if (status == PWT_HCI_NO_DATA) {
        return;
    }
    if (status != PWT_HCI_OK) {
        bridge->faulted = true;
        return;
    }
    if (!controller_packet_valid(&slot->packet)) {
        bridge->stats.malformed_controller_packets++;
        bridge->faulted = true;
        return;
    }

    process_flow_event(bridge, &slot->packet);
    bridge->rx_tail =
        (uint8_t)((bridge->rx_tail + 1u) % PWT_BRIDGE_RX_QUEUE_CAPACITY);
    bridge->rx_count++;
    bridge->stats.controller_rx_received++;
}

static void pop_command(pwt_controller_bridge_t *bridge) {
    memset(&bridge->command_queue[bridge->command_head], 0, sizeof(pwt_bridge_slot_t));
    bridge->command_head =
        (uint8_t)((bridge->command_head + 1u) % PWT_BRIDGE_COMMAND_QUEUE_CAPACITY);
    bridge->command_count--;
}

static void pop_acl(pwt_controller_bridge_t *bridge) {
    memset(&bridge->acl_queue[bridge->acl_head], 0, sizeof(pwt_bridge_slot_t));
    bridge->acl_head =
        (uint8_t)((bridge->acl_head + 1u) % PWT_BRIDGE_ACL_QUEUE_CAPACITY);
    bridge->acl_count--;
}

static void service_send(pwt_controller_bridge_t *bridge) {
    if (bridge->wait_kind != PWT_BRIDGE_WAIT_NONE) {
        bridge->stats.controller_waits++;
        return;
    }

    pwt_hci_packet_t *packet = NULL;
    if (bridge->command_count > 0u) {
        packet = &bridge->command_queue[bridge->command_head].packet;
    } else if (bridge->acl_count > 0u) {
        packet = &bridge->acl_queue[bridge->acl_head].packet;
    } else {
        return;
    }

    const pwt_hci_status_t status =
        bridge->link_ops->send(bridge->link_context, packet);
    if (status != PWT_HCI_OK) {
        bridge->faulted = true;
        return;
    }

    if (packet->kind == PWT_HCI_PACKET_COMMAND) {
        bridge->wait_kind = PWT_BRIDGE_WAIT_COMMAND;
        bridge->wait_identifier =
            (uint16_t)((uint16_t)packet->payload[0] |
                       ((uint16_t)packet->payload[1] << 8u));
        pop_command(bridge);
    } else {
        bridge->wait_kind = PWT_BRIDGE_WAIT_ACL;
        bridge->wait_identifier =
            (uint16_t)(((uint16_t)packet->payload[0] |
                        ((uint16_t)packet->payload[1] << 8u)) &
                       0x0fffu);
        pop_acl(bridge);
    }
    bridge->stats.controller_tx_sent++;
}

void pwt_controller_bridge_init_with_link(
    pwt_controller_bridge_t *bridge,
    const pwt_hci_link_ops_t *link_ops,
    void *link_context) {
    if (bridge == NULL) {
        return;
    }
    memset(bridge, 0, sizeof(*bridge));
    bridge->link_ops = link_ops;
    bridge->link_context = link_context;
    bridge->faulted = !link_valid(link_ops) || link_context == NULL;
}

void pwt_controller_bridge_init(
    pwt_controller_bridge_t *bridge,
    pwt_hci_transport_t *transport) {
    pwt_controller_bridge_init_with_link(
        bridge, &production_link_ops, transport);
}

pwt_controller_backend_t pwt_controller_bridge_backend(
    pwt_controller_bridge_t *bridge) {
    const pwt_controller_backend_t backend = {
        .context = bridge,
        .submit_from_host = bridge_submit_from_host,
        .can_accept_from_host = bridge_can_accept_from_host,
        .peek_to_host = bridge_peek_to_host,
        .release_to_host = bridge_release_to_host,
        .reset = bridge_backend_reset,
    };
    return backend;
}

void pwt_controller_bridge_service(pwt_controller_bridge_t *bridge) {
    if (bridge == NULL || bridge->faulted || !link_valid(bridge->link_ops)) {
        return;
    }

    /*
     * Read before write so a controller completion can release the conservative
     * one-packet CYW43 TX pacing gate in the same service iteration.
     */
    service_receive(bridge);
    if (!bridge->faulted) {
        service_send(bridge);
    }
}

pwt_hci_status_t pwt_controller_bridge_reset(pwt_controller_bridge_t *bridge) {
    if (bridge == NULL || !link_valid(bridge->link_ops)) {
        return PWT_HCI_E_ARGUMENT;
    }

    bridge->stats.packets_discarded_on_reset +=
        bridge->command_count + bridge->acl_count + bridge->rx_count;
    bridge->stats.resets++;
    clear_queues(bridge);

    const pwt_hci_status_t status =
        bridge->link_ops->reset(bridge->link_context);
    bridge->faulted = status != PWT_HCI_OK;
    if (bridge->faulted) {
        bridge->stats.recovery_failures++;
    }
    return status;
}

bool pwt_controller_bridge_needs_recovery(const pwt_controller_bridge_t *bridge) {
    return bridge == NULL || bridge->faulted;
}

const pwt_controller_bridge_stats_t *pwt_controller_bridge_stats(
    const pwt_controller_bridge_t *bridge) {
    return bridge == NULL ? NULL : &bridge->stats;
}
