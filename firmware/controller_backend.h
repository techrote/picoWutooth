#ifndef PWT_CONTROLLER_BACKEND_H
#define PWT_CONTROLLER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "picowutooth/hci_transport.h"

typedef enum {
    PWT_BACKEND_ACCEPTED = 0,
    PWT_BACKEND_BUSY,
    PWT_BACKEND_REJECTED,
} pwt_backend_submit_result_t;

typedef struct {
    pwt_hci_packet_kind_t kind;
    const uint8_t *data;
    uint16_t length;
    const void *token;
} pwt_hci_packet_view_t;

typedef struct {
    void *context;

    /*
     * TinyUSB owns host->controller callback buffers. Implementations must
     * synchronously consume/copy data before returning ACCEPTED.
     */
    pwt_backend_submit_result_t (*submit_from_host)(
        void *context,
        pwt_hci_packet_kind_t kind,
        const uint8_t *data,
        uint16_t length);

    /*
     * Allows the USB ingress side to stop arming OUT transfers before the next
     * packet would overflow bounded controller-facing storage.
     */
    bool (*can_accept_from_host)(void *context, pwt_hci_packet_kind_t kind);

    /*
     * Returns one controller->host packet whose backing storage remains owned
     * by the backend until release_to_host() is called with the same token.
     */
    bool (*peek_to_host)(void *context, pwt_hci_packet_view_t *packet);
    void (*release_to_host)(void *context, const void *token);

    /* USB lifecycle reset notification; implementations must be idempotent. */
    void (*reset)(void *context);
} pwt_controller_backend_t;

#endif
