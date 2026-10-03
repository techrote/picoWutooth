#include "usb_hci_router.h"

#include <string.h>

static bool router_valid(const pwt_usb_hci_router_t *router) {
    return router != NULL && router->backend.submit_from_host != NULL &&
           router->backend.peek_to_host != NULL && router->backend.release_to_host != NULL &&
           router->usb_tx.send_event != NULL && router->usb_tx.send_acl != NULL;
}

void pwt_usb_hci_router_init(
    pwt_usb_hci_router_t *router,
    pwt_controller_backend_t backend,
    pwt_usb_tx_t usb_tx) {
    if (router == NULL) {
        return;
    }

    memset(router, 0, sizeof(*router));
    router->backend = backend;
    router->usb_tx = usb_tx;
}

static pwt_backend_submit_result_t receive_from_host(
    pwt_usb_hci_router_t *router,
    pwt_hci_packet_kind_t kind,
    const uint8_t *data,
    uint16_t length) {
    if (!router_valid(router) || data == NULL || length == 0) {
        if (router != NULL) {
            router->stats.backend_rejected++;
        }
        return PWT_BACKEND_REJECTED;
    }

    const pwt_backend_submit_result_t result =
        router->backend.submit_from_host(router->backend.context, kind, data, length);

    if (result == PWT_BACKEND_ACCEPTED) {
        if (kind == PWT_HCI_PACKET_COMMAND) {
            router->stats.host_commands_accepted++;
        } else {
            router->stats.host_acl_accepted++;
        }
    } else if (result == PWT_BACKEND_BUSY) {
        router->stats.backend_busy++;
    } else {
        router->stats.backend_rejected++;
    }

    return result;
}

pwt_backend_submit_result_t pwt_usb_hci_receive_command(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length) {
    return receive_from_host(router, PWT_HCI_PACKET_COMMAND, data, length);
}

pwt_backend_submit_result_t pwt_usb_hci_receive_acl(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length) {
    return receive_from_host(router, PWT_HCI_PACKET_ACL, data, length);
}

static void release_pending(pwt_usb_hci_router_t *router) {
    if (!router->pending_valid) {
        return;
    }

    router->backend.release_to_host(router->backend.context, router->pending.token);
    memset(&router->pending, 0, sizeof(router->pending));
    router->pending_valid = false;
    router->pending_submitted = false;
}

void pwt_usb_hci_service(pwt_usb_hci_router_t *router) {
    if (!router_valid(router) || router->pending_submitted) {
        return;
    }

    if (!router->pending_valid) {
        pwt_hci_packet_view_t packet = {0};
        if (!router->backend.peek_to_host(router->backend.context, &packet)) {
            return;
        }

        if (packet.data == NULL || packet.length == 0 || packet.token == NULL ||
            (packet.kind != PWT_HCI_PACKET_EVENT && packet.kind != PWT_HCI_PACKET_ACL)) {
            router->stats.malformed_controller_packets++;
            if (packet.token != NULL) {
                router->backend.release_to_host(router->backend.context, packet.token);
            }
            return;
        }

        router->pending = packet;
        router->pending_valid = true;
    }

    bool submitted = false;
    if (router->pending.kind == PWT_HCI_PACKET_EVENT) {
        submitted = router->usb_tx.send_event(
            router->usb_tx.context, router->pending.data, router->pending.length);
    } else {
        submitted = router->usb_tx.send_acl(
            router->usb_tx.context, router->pending.data, router->pending.length);
    }

    if (submitted) {
        router->pending_submitted = true;
    } else {
        router->stats.usb_busy_retries++;
    }
}

static void transfer_complete(
    pwt_usb_hci_router_t *router,
    pwt_hci_packet_kind_t kind,
    uint16_t sent_bytes) {
    if (router == NULL || !router->pending_valid || !router->pending_submitted ||
        router->pending.kind != kind) {
        if (router != NULL) {
            router->stats.completion_mismatch++;
        }
        return;
    }

    if (sent_bytes != router->pending.length) {
        router->stats.completion_mismatch++;
    }

    router->stats.controller_packets_sent++;
    release_pending(router);
}

void pwt_usb_hci_event_sent(pwt_usb_hci_router_t *router, uint16_t sent_bytes) {
    transfer_complete(router, PWT_HCI_PACKET_EVENT, sent_bytes);
}

void pwt_usb_hci_acl_sent(pwt_usb_hci_router_t *router, uint16_t sent_bytes) {
    transfer_complete(router, PWT_HCI_PACKET_ACL, sent_bytes);
}

void pwt_usb_hci_reset(pwt_usb_hci_router_t *router) {
    if (router == NULL) {
        return;
    }

    release_pending(router);
    if (router->backend.reset != NULL) {
        router->backend.reset(router->backend.context);
    }
    router->stats.resets++;
}

const pwt_usb_hci_stats_t *pwt_usb_hci_stats(const pwt_usb_hci_router_t *router) {
    return router == NULL ? NULL : &router->stats;
}
