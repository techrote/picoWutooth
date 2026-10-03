#include "controller_stub.h"

#include <string.h>

static pwt_backend_submit_result_t stub_submit_from_host(
    void *context,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length) {
    pwt_controller_stub_t *stub = context;
    if (stub == NULL || data == NULL || length == 0 || length > PWT_STUB_CAPTURE_CAPACITY ||
        (kind != PWT_HCI_PACKET_COMMAND && kind != PWT_HCI_PACKET_ACL)) {
        return PWT_BACKEND_REJECTED;
    }

    memcpy(stub->last_host_data, data, length);
    stub->last_host_length = length;
    stub->last_host_kind = kind;
    stub->host_submit_count++;
    return PWT_BACKEND_ACCEPTED;
}

static bool stub_peek_to_host(void *context, pwt_hci_packet_view_t *packet) {
    pwt_controller_stub_t *stub = context;
    if (stub == NULL || packet == NULL || !stub->to_host_queued) {
        return false;
    }

    stub->to_host_leased = true;
    packet->kind = stub->to_host_kind;
    packet->data = stub->to_host_data;
    packet->length = stub->to_host_length;
    packet->token = stub;
    return true;
}

static void stub_release_to_host(void *context, const void *token) {
    pwt_controller_stub_t *stub = context;
    if (stub == NULL || token != stub || !stub->to_host_queued) {
        return;
    }

    stub->to_host_queued = false;
    stub->to_host_leased = false;
    stub->to_host_length = 0;
    stub->release_count++;
}

static void stub_reset(void *context) {
    pwt_controller_stub_t *stub = context;
    if (stub == NULL) {
        return;
    }

    stub->to_host_queued = false;
    stub->to_host_leased = false;
    stub->to_host_length = 0;
    stub->reset_count++;
}

void pwt_controller_stub_init(pwt_controller_stub_t *stub) {
    if (stub != NULL) {
        memset(stub, 0, sizeof(*stub));
    }
}

pwt_controller_backend_t pwt_controller_stub_backend(pwt_controller_stub_t *stub) {
    const pwt_controller_backend_t backend = {
        .context = stub,
        .submit_from_host = stub_submit_from_host,
        .peek_to_host = stub_peek_to_host,
        .release_to_host = stub_release_to_host,
        .reset = stub_reset,
    };
    return backend;
}

bool pwt_controller_stub_queue_to_host(
    pwt_controller_stub_t *stub,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length) {
    if (stub == NULL || data == NULL || length == 0 || length > PWT_STUB_TO_HOST_CAPACITY ||
        stub->to_host_queued || (kind != PWT_HCI_PACKET_EVENT && kind != PWT_HCI_PACKET_ACL)) {
        return false;
    }

    memcpy(stub->to_host_data, data, length);
    stub->to_host_kind = kind;
    stub->to_host_length = length;
    stub->to_host_queued = true;
    stub->to_host_leased = false;
    return true;
}
