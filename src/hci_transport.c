#include "picowutooth/hci_transport.h"

#include <string.h>

#define PWT_HCI_EVENT_COMMAND_COMPLETE 0x0eu
#define PWT_HCI_OPCODE_RESET 0x0c03u
#define PWT_HCI_OPCODE_CYW43_WRITE_BD_ADDR 0xfc01u
#define PWT_INIT_WAIT_ITERATIONS 1000u

static bool pwt_kind_is_valid(uint8_t kind) {
    return kind == (uint8_t)PWT_HCI_PACKET_COMMAND ||
           kind == (uint8_t)PWT_HCI_PACKET_ACL ||
           kind == (uint8_t)PWT_HCI_PACKET_EVENT;
}

static bool pwt_backend_is_valid(const pwt_hci_backend_ops_t *backend) {
    return backend != NULL && backend->start != NULL && backend->stop != NULL &&
           backend->write_raw != NULL && backend->read_raw != NULL &&
           backend->get_wlan_mac != NULL && backend->delay_ms != NULL;
}

static void pwt_clear_private_buffers(pwt_hci_transport_t *transport) {
    memset(transport->tx_frame, 0, sizeof(transport->tx_frame));
    memset(transport->rx_frame, 0, sizeof(transport->rx_frame));
}

static pwt_hci_status_t pwt_encode_frame(
    pwt_hci_transport_t *transport,
    pwt_hci_packet_kind_t kind,
    const uint8_t *payload,
    size_t payload_length,
    size_t *frame_length) {
    if (!pwt_kind_is_valid((uint8_t)kind) || frame_length == NULL ||
        (payload_length != 0u && payload == NULL)) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (payload_length > PWT_HCI_MAX_PAYLOAD) {
        return PWT_HCI_E_OVERSIZE;
    }

    transport->tx_frame[0] = (uint8_t)(payload_length & 0xffu);
    transport->tx_frame[1] = (uint8_t)((payload_length >> 8u) & 0xffu);
    transport->tx_frame[2] = (uint8_t)((payload_length >> 16u) & 0xffu);
    transport->tx_frame[3] = (uint8_t)kind;
    if (payload_length != 0u) {
        memcpy(&transport->tx_frame[PWT_CYW43_HEADER_SIZE], payload, payload_length);
    }
    *frame_length = payload_length + PWT_CYW43_HEADER_SIZE;
    return PWT_HCI_OK;
}

static pwt_hci_status_t pwt_decode_frame(
    const uint8_t *frame,
    size_t frame_length,
    pwt_hci_packet_t *packet) {
    if (frame == NULL || packet == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (frame_length < PWT_CYW43_HEADER_SIZE) {
        return PWT_HCI_E_MALFORMED;
    }

    const size_t declared_length = (size_t)frame[0] |
                                   ((size_t)frame[1] << 8u) |
                                   ((size_t)frame[2] << 16u);
    const size_t actual_length = frame_length - PWT_CYW43_HEADER_SIZE;
    if (declared_length != actual_length) {
        return PWT_HCI_E_MALFORMED;
    }
    if (declared_length > PWT_HCI_MAX_PAYLOAD) {
        return PWT_HCI_E_OVERSIZE;
    }
    if (!pwt_kind_is_valid(frame[3])) {
        return PWT_HCI_E_MALFORMED;
    }

    packet->kind = (pwt_hci_packet_kind_t)frame[3];
    packet->length = (uint16_t)declared_length;
    if (declared_length != 0u) {
        memcpy(packet->payload, &frame[PWT_CYW43_HEADER_SIZE], declared_length);
    }
    return PWT_HCI_OK;
}

static pwt_hci_status_t pwt_write_bootstrap_command(
    pwt_hci_transport_t *transport,
    const uint8_t *command,
    size_t command_length) {
    size_t frame_length = 0u;
    pwt_hci_status_t status = pwt_encode_frame(
        transport, PWT_HCI_PACKET_COMMAND, command, command_length, &frame_length);
    if (status != PWT_HCI_OK) {
        return status;
    }
    if (transport->backend->write_raw(
            transport->backend_context, transport->tx_frame, frame_length) != 0) {
        return PWT_HCI_E_TRANSPORT;
    }
    return PWT_HCI_OK;
}

static pwt_hci_status_t pwt_read_raw_packet(
    pwt_hci_transport_t *transport,
    pwt_hci_packet_t *packet) {
    size_t frame_length = 0u;
    if (transport->backend->read_raw(
            transport->backend_context,
            transport->rx_frame,
            sizeof(transport->rx_frame),
            &frame_length) != 0) {
        return PWT_HCI_E_TRANSPORT;
    }
    if (frame_length == 0u) {
        return PWT_HCI_NO_DATA;
    }
    if (frame_length > sizeof(transport->rx_frame)) {
        return PWT_HCI_E_OVERSIZE;
    }
    return pwt_decode_frame(transport->rx_frame, frame_length, packet);
}

static pwt_hci_status_t pwt_wait_command_complete(
    pwt_hci_transport_t *transport,
    uint16_t expected_opcode) {
    for (uint32_t attempt = 0u; attempt < PWT_INIT_WAIT_ITERATIONS; ++attempt) {
        pwt_hci_packet_t packet;
        pwt_hci_status_t status = pwt_read_raw_packet(transport, &packet);
        if (status == PWT_HCI_NO_DATA) {
            transport->backend->delay_ms(transport->backend_context, 1u);
            continue;
        }
        if (status != PWT_HCI_OK) {
            return status;
        }
        if (packet.kind != PWT_HCI_PACKET_EVENT) {
            return PWT_HCI_E_MALFORMED;
        }
        if (packet.length < 2u || (size_t)packet.payload[1] + 2u != packet.length) {
            return PWT_HCI_E_MALFORMED;
        }
        if (packet.payload[0] != PWT_HCI_EVENT_COMMAND_COMPLETE) {
            continue;
        }
        if (packet.length < 6u || packet.payload[1] < 4u) {
            return PWT_HCI_E_MALFORMED;
        }

        const uint16_t opcode = (uint16_t)packet.payload[3] |
                                ((uint16_t)packet.payload[4] << 8u);
        if (opcode != expected_opcode) {
            continue;
        }
        return packet.payload[5] == 0u ? PWT_HCI_OK : PWT_HCI_E_CONTROLLER;
    }
    return PWT_HCI_E_TIMEOUT;
}

static pwt_hci_status_t pwt_bootstrap_controller(pwt_hci_transport_t *transport) {
    static const uint8_t reset_command[] = {0x03u, 0x0cu, 0x00u};

    uint8_t wlan_mac[PWT_BD_ADDR_LEN];
    if (transport->backend->get_wlan_mac(transport->backend_context, wlan_mac) != 0) {
        return PWT_HCI_E_TRANSPORT;
    }
    memcpy(transport->bd_addr, wlan_mac, sizeof(transport->bd_addr));
    transport->bd_addr[PWT_BD_ADDR_LEN - 1u] =
        (uint8_t)(transport->bd_addr[PWT_BD_ADDR_LEN - 1u] + 1u);

    pwt_hci_status_t status = pwt_write_bootstrap_command(
        transport, reset_command, sizeof(reset_command));
    if (status != PWT_HCI_OK) {
        return status;
    }
    status = pwt_wait_command_complete(transport, PWT_HCI_OPCODE_RESET);
    if (status != PWT_HCI_OK) {
        return status;
    }

    uint8_t set_address_command[9] = {0x01u, 0xfcu, 0x06u};
    for (size_t i = 0u; i < PWT_BD_ADDR_LEN; ++i) {
        set_address_command[3u + i] = transport->bd_addr[PWT_BD_ADDR_LEN - 1u - i];
    }
    status = pwt_write_bootstrap_command(
        transport, set_address_command, sizeof(set_address_command));
    if (status != PWT_HCI_OK) {
        return status;
    }
    return pwt_wait_command_complete(transport, PWT_HCI_OPCODE_CYW43_WRITE_BD_ADDR);
}

static pwt_hci_status_t pwt_start(pwt_hci_transport_t *transport) {
    transport->state = PWT_HCI_STATE_STARTING;
    pwt_clear_private_buffers(transport);
    memset(transport->bd_addr, 0, sizeof(transport->bd_addr));

    if (transport->backend->start(transport->backend_context) != 0) {
        transport->state = PWT_HCI_STATE_ERROR;
        return PWT_HCI_E_TRANSPORT;
    }
    transport->backend_started = true;

    pwt_hci_status_t status = pwt_bootstrap_controller(transport);
    if (status != PWT_HCI_OK) {
        transport->state = PWT_HCI_STATE_ERROR;
        return status;
    }
    transport->state = PWT_HCI_STATE_READY;
    return PWT_HCI_OK;
}

pwt_hci_status_t pwt_hci_transport_init(
    pwt_hci_transport_t *transport,
    const pwt_hci_backend_ops_t *backend,
    void *backend_context) {
    if (transport == NULL || !pwt_backend_is_valid(backend)) {
        return PWT_HCI_E_ARGUMENT;
    }
    memset(transport, 0, sizeof(*transport));
    transport->backend = backend;
    transport->backend_context = backend_context;
    return pwt_start(transport);
}

pwt_hci_status_t pwt_hci_transport_send(
    pwt_hci_transport_t *transport,
    const pwt_hci_packet_t *packet) {
    if (transport == NULL || packet == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (transport->state != PWT_HCI_STATE_READY) {
        return PWT_HCI_E_NOT_READY;
    }
    if (packet->kind != PWT_HCI_PACKET_COMMAND && packet->kind != PWT_HCI_PACKET_ACL) {
        return PWT_HCI_E_DIRECTION;
    }
    if (packet->length > PWT_HCI_MAX_PAYLOAD) {
        return PWT_HCI_E_OVERSIZE;
    }

    size_t frame_length = 0u;
    pwt_hci_status_t status = pwt_encode_frame(
        transport, packet->kind, packet->payload, packet->length, &frame_length);
    if (status != PWT_HCI_OK) {
        return status;
    }
    if (transport->backend->write_raw(
            transport->backend_context, transport->tx_frame, frame_length) != 0) {
        transport->state = PWT_HCI_STATE_ERROR;
        return PWT_HCI_E_TRANSPORT;
    }
    return PWT_HCI_OK;
}

pwt_hci_status_t pwt_hci_transport_receive(
    pwt_hci_transport_t *transport,
    pwt_hci_packet_t *packet) {
    if (transport == NULL || packet == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (transport->state != PWT_HCI_STATE_READY) {
        return PWT_HCI_E_NOT_READY;
    }

    pwt_hci_status_t status = pwt_read_raw_packet(transport, packet);
    if (status == PWT_HCI_NO_DATA) {
        return status;
    }
    if (status != PWT_HCI_OK) {
        transport->state = PWT_HCI_STATE_ERROR;
        return status;
    }
    if (packet->kind != PWT_HCI_PACKET_EVENT && packet->kind != PWT_HCI_PACKET_ACL) {
        transport->state = PWT_HCI_STATE_ERROR;
        return PWT_HCI_E_DIRECTION;
    }
    return PWT_HCI_OK;
}

pwt_hci_status_t pwt_hci_transport_reset(pwt_hci_transport_t *transport) {
    if (transport == NULL || !pwt_backend_is_valid(transport->backend)) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (transport->backend_started) {
        transport->backend->stop(transport->backend_context);
        transport->backend_started = false;
    }
    return pwt_start(transport);
}

void pwt_hci_transport_deinit(pwt_hci_transport_t *transport) {
    if (transport == NULL) {
        return;
    }
    if (transport->backend_started && transport->backend != NULL && transport->backend->stop != NULL) {
        transport->backend->stop(transport->backend_context);
        transport->backend_started = false;
    }
    pwt_clear_private_buffers(transport);
    memset(transport->bd_addr, 0, sizeof(transport->bd_addr));
    transport->state = PWT_HCI_STATE_DOWN;
}

pwt_hci_state_t pwt_hci_transport_state(const pwt_hci_transport_t *transport) {
    return transport == NULL ? PWT_HCI_STATE_ERROR : transport->state;
}

pwt_hci_status_t pwt_hci_transport_bd_addr(
    const pwt_hci_transport_t *transport,
    uint8_t bd_addr[PWT_BD_ADDR_LEN]) {
    if (transport == NULL || bd_addr == NULL) {
        return PWT_HCI_E_ARGUMENT;
    }
    if (transport->state != PWT_HCI_STATE_READY) {
        return PWT_HCI_E_NOT_READY;
    }
    memcpy(bd_addr, transport->bd_addr, PWT_BD_ADDR_LEN);
    return PWT_HCI_OK;
}
