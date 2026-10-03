#ifndef PWT_CONTROLLER_STUB_H
#define PWT_CONTROLLER_STUB_H

#include "controller_backend.h"

#define PWT_STUB_CAPTURE_CAPACITY 258u
#define PWT_STUB_TO_HOST_CAPACITY 260u

typedef struct {
    uint8_t last_host_data[PWT_STUB_CAPTURE_CAPACITY];
    uint16_t last_host_length;
    pwt_hci_packet_kind_t last_host_kind;
    uint32_t host_submit_count;

    uint8_t to_host_data[PWT_STUB_TO_HOST_CAPACITY];
    uint16_t to_host_length;
    pwt_hci_packet_kind_t to_host_kind;
    bool to_host_queued;
    bool to_host_leased;

    uint32_t release_count;
    uint32_t reset_count;
} pwt_controller_stub_t;

void pwt_controller_stub_init(pwt_controller_stub_t *stub);
pwt_controller_backend_t pwt_controller_stub_backend(pwt_controller_stub_t *stub);

bool pwt_controller_stub_queue_to_host(
    pwt_controller_stub_t *stub,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length);

#endif
