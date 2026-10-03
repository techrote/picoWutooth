#ifndef PWT_USB_HCI_ROUTER_H
#define PWT_USB_HCI_ROUTER_H

#include "controller_backend.h"

#define PWT_USB_HCI_MAX_COMMAND 258u

typedef struct {
    void *context;
    bool (*send_event)(void *context, const uint8_t *data, uint16_t length);
    bool (*send_acl)(void *context, const uint8_t *data, uint16_t length);
} pwt_usb_tx_t;

typedef struct {
    uint32_t host_commands_accepted;
    uint32_t host_commands_buffered;
    uint32_t host_acl_accepted;
    uint32_t host_acl_buffered;
    uint32_t host_acl_fragments;
    uint32_t backend_busy;
    uint32_t backend_rejected;
    uint32_t host_ingress_busy_retries;
    uint32_t malformed_host_packets;
    uint32_t controller_packets_sent;
    uint32_t usb_busy_retries;
    uint32_t malformed_controller_packets;
    uint32_t completion_mismatch;
    uint32_t packets_discarded_on_reset;
    uint32_t resets;
} pwt_usb_hci_stats_t;

typedef struct {
    pwt_controller_backend_t backend;
    pwt_usb_tx_t usb_tx;

    uint8_t pending_command[PWT_USB_HCI_MAX_COMMAND];
    uint16_t pending_command_length;
    bool pending_command_valid;

    uint8_t acl_assembly[PWT_HCI_MAX_PAYLOAD];
    uint16_t acl_assembly_length;
    uint16_t acl_expected_length;
    bool acl_complete_pending;

    pwt_hci_packet_view_t pending;
    bool pending_valid;
    bool pending_submitted;
    bool recovery_required;

    pwt_usb_hci_stats_t stats;
} pwt_usb_hci_router_t;

void pwt_usb_hci_router_init(
    pwt_usb_hci_router_t *router,
    pwt_controller_backend_t backend,
    pwt_usb_tx_t usb_tx);

pwt_backend_submit_result_t pwt_usb_hci_receive_command(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length);

/*
 * Accepts one USB bulk-OUT transfer fragment. A complete HCI ACL packet may
 * span several calls. BUSY means the completed packet is safely retained and
 * the USB endpoint must not be re-armed until service() makes space.
 */
pwt_backend_submit_result_t pwt_usb_hci_receive_acl(
    pwt_usb_hci_router_t *router, const uint8_t *data, uint16_t length);

bool pwt_usb_hci_can_receive_acl(const pwt_usb_hci_router_t *router);

void pwt_usb_hci_service(pwt_usb_hci_router_t *router);
void pwt_usb_hci_event_sent(pwt_usb_hci_router_t *router, uint16_t sent_bytes);
void pwt_usb_hci_acl_sent(pwt_usb_hci_router_t *router, uint16_t sent_bytes);
void pwt_usb_hci_reset(pwt_usb_hci_router_t *router);

bool pwt_usb_hci_recovery_required(const pwt_usb_hci_router_t *router);
const pwt_usb_hci_stats_t *pwt_usb_hci_stats(const pwt_usb_hci_router_t *router);

#endif
