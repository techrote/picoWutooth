#include "usb_hci_router.h"

#include <string.h>

static bool router_valid(const pwt_usb_hci_router_t *router) {
    return router != NULL && router->backend.submit_from_host != NULL &&
           router->backend.can_accept_from_host != NULL &&
           router->backend.peek_to_host != NULL && router->backend.release_to_host != NULL &&
           router->usb_tx.send_event != NULL && router->usb_tx.send_acl != NULL;
}

static bool command_valid(const uint8_t *data, uint16_t length) {
    return data != NULL && length >= 3u && length <= PWT_USB_HCI_MAX_COMMAND &&
           (uint16_t)((uint16_t)data[2] + 3u) == length;
}

static void clear_acl_assembly(pwt_usb_hci_router_t *router) {
    memset(router->acl_assembly, 0, sizeof(router->acl_assembly));
    router->acl_assembly_length = 0u;
    router->acl_expected_length = 0u;
    router->acl_complete_pending = false;
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

pwt_backend_submit_result_t pwt_usb_hci_receive_command(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length) {
    if (!router_valid(router) || !command_valid(data, length) ||
        router->recovery_required) {
        if (router != NULL) {
            router->stats.malformed_host_packets++;
            router->stats.backend_rejected++;
            router->recovery_required = true;
        }
        return PWT_BACKEND_REJECTED;
    }

    if (router->pending_command_valid) {
        router->stats.backend_busy++;
        router->recovery_required = true;
        return PWT_BACKEND_BUSY;
    }

    if (router->backend.can_accept_from_host(
            router->backend.context, PWT_HCI_PACKET_COMMAND)) {
        const pwt_backend_submit_result_t result =
            router->backend.submit_from_host(
                router->backend.context, PWT_HCI_PACKET_COMMAND, data, length);
        if (result == PWT_BACKEND_ACCEPTED) {
            router->stats.host_commands_accepted++;
            return result;
        }
        if (result == PWT_BACKEND_REJECTED) {
            router->stats.backend_rejected++;
            router->recovery_required = true;
            return result;
        }
    }

    memcpy(router->pending_command, data, length);
    router->pending_command_length = length;
    router->pending_command_valid = true;
    router->stats.host_commands_buffered++;
    router->stats.backend_busy++;
    return PWT_BACKEND_ACCEPTED;
}

static pwt_backend_submit_result_t submit_complete_acl(pwt_usb_hci_router_t *router) {
    if (!router->backend.can_accept_from_host(
            router->backend.context, PWT_HCI_PACKET_ACL)) {
        router->acl_complete_pending = true;
        router->stats.backend_busy++;
        router->stats.host_acl_buffered++;
        return PWT_BACKEND_BUSY;
    }

    const pwt_backend_submit_result_t result =
        router->backend.submit_from_host(
            router->backend.context,
            PWT_HCI_PACKET_ACL,
            router->acl_assembly,
            router->acl_assembly_length);
    if (result == PWT_BACKEND_ACCEPTED) {
        router->stats.host_acl_accepted++;
        clear_acl_assembly(router);
    } else if (result == PWT_BACKEND_BUSY) {
        router->acl_complete_pending = true;
        router->stats.backend_busy++;
        router->stats.host_acl_buffered++;
    } else {
        router->stats.backend_rejected++;
        router->recovery_required = true;
    }
    return result;
}

pwt_backend_submit_result_t pwt_usb_hci_receive_acl(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length) {
    if (!router_valid(router) || data == NULL || length == 0u ||
        router->recovery_required) {
        if (router != NULL) {
            router->stats.malformed_host_packets++;
            router->stats.backend_rejected++;
            router->recovery_required = true;
        }
        return PWT_BACKEND_REJECTED;
    }

    if (router->acl_complete_pending) {
        router->stats.backend_busy++;
        return PWT_BACKEND_BUSY;
    }

    const uint32_t new_length =
        (uint32_t)router->acl_assembly_length + (uint32_t)length;
    if (new_length > PWT_HCI_MAX_PAYLOAD) {
        router->stats.malformed_host_packets++;
        router->stats.backend_rejected++;
        router->recovery_required = true;
        clear_acl_assembly(router);
        return PWT_BACKEND_REJECTED;
    }

    memcpy(
        &router->acl_assembly[router->acl_assembly_length],
        data,
        length);
    router->acl_assembly_length = (uint16_t)new_length;
    router->stats.host_acl_fragments++;

    if (router->acl_expected_length == 0u && router->acl_assembly_length >= 4u) {
        const uint16_t payload_length =
            (uint16_t)((uint16_t)router->acl_assembly[2] |
                       ((uint16_t)router->acl_assembly[3] << 8u));
        const uint32_t expected = (uint32_t)payload_length + 4u;
        if (expected > PWT_HCI_MAX_PAYLOAD) {
            router->stats.malformed_host_packets++;
            router->stats.backend_rejected++;
            router->recovery_required = true;
            clear_acl_assembly(router);
            return PWT_BACKEND_REJECTED;
        }
        router->acl_expected_length = (uint16_t)expected;
    }

    if (router->acl_expected_length == 0u ||
        router->acl_assembly_length < router->acl_expected_length) {
        return PWT_BACKEND_ACCEPTED;
    }

    if (router->acl_assembly_length > router->acl_expected_length) {
        router->stats.malformed_host_packets++;
        router->stats.backend_rejected++;
        router->recovery_required = true;
        clear_acl_assembly(router);
        return PWT_BACKEND_REJECTED;
    }

    return submit_complete_acl(router);
}

bool pwt_usb_hci_can_receive_acl(const pwt_usb_hci_router_t *router) {
    return router_valid(router) && !router->recovery_required &&
           !router->acl_complete_pending &&
           router->acl_assembly_length < PWT_HCI_MAX_PAYLOAD;
}

static void retry_pending_host(pwt_usb_hci_router_t *router) {
    if (router->pending_command_valid &&
        router->backend.can_accept_from_host(
            router->backend.context, PWT_HCI_PACKET_COMMAND)) {
        const pwt_backend_submit_result_t result =
            router->backend.submit_from_host(
                router->backend.context,
                PWT_HCI_PACKET_COMMAND,
                router->pending_command,
                router->pending_command_length);
        if (result == PWT_BACKEND_ACCEPTED) {
            memset(router->pending_command, 0, sizeof(router->pending_command));
            router->pending_command_length = 0u;
            router->pending_command_valid = false;
            router->stats.host_commands_accepted++;
        } else if (result == PWT_BACKEND_REJECTED) {
            router->stats.backend_rejected++;
            router->recovery_required = true;
            return;
        } else {
            router->stats.host_ingress_busy_retries++;
        }
    }

    if (router->acl_complete_pending &&
        router->backend.can_accept_from_host(
            router->backend.context, PWT_HCI_PACKET_ACL)) {
        const pwt_backend_submit_result_t result =
            router->backend.submit_from_host(
                router->backend.context,
                PWT_HCI_PACKET_ACL,
                router->acl_assembly,
                router->acl_assembly_length);
        if (result == PWT_BACKEND_ACCEPTED) {
            router->stats.host_acl_accepted++;
            clear_acl_assembly(router);
        } else if (result == PWT_BACKEND_REJECTED) {
            router->stats.backend_rejected++;
            router->recovery_required = true;
        } else {
            router->stats.host_ingress_busy_retries++;
        }
    }
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
    if (!router_valid(router) || router->recovery_required) {
        return;
    }

    retry_pending_host(router);
    if (router->recovery_required || router->pending_submitted) {
        return;
    }

    if (!router->pending_valid) {
        pwt_hci_packet_view_t packet = {0};
        if (!router->backend.peek_to_host(router->backend.context, &packet)) {
            return;
        }

        if (packet.data == NULL || packet.length == 0u || packet.token == NULL ||
            (packet.kind != PWT_HCI_PACKET_EVENT && packet.kind != PWT_HCI_PACKET_ACL)) {
            router->stats.malformed_controller_packets++;
            router->recovery_required = true;
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
            router->recovery_required = true;
        }
        return;
    }

    if (sent_bytes != router->pending.length) {
        router->stats.completion_mismatch++;
        router->recovery_required = true;
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

    if (router->pending_command_valid) {
        router->stats.packets_discarded_on_reset++;
    }
    if (router->acl_assembly_length != 0u) {
        router->stats.packets_discarded_on_reset++;
    }

    memset(router->pending_command, 0, sizeof(router->pending_command));
    router->pending_command_length = 0u;
    router->pending_command_valid = false;
    clear_acl_assembly(router);

    if (router->backend.reset != NULL) {
        router->backend.reset(router->backend.context);
    }
    router->recovery_required = false;
    router->stats.resets++;
}

bool pwt_usb_hci_recovery_required(const pwt_usb_hci_router_t *router) {
    return router == NULL || router->recovery_required;
}

const pwt_usb_hci_stats_t *pwt_usb_hci_stats(const pwt_usb_hci_router_t *router) {
    return router == NULL ? NULL : &router->stats;
}
