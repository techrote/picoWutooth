#ifndef PWT_CONTROLLER_BRIDGE_H
#define PWT_CONTROLLER_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "controller_backend.h"

#define PWT_BRIDGE_COMMAND_QUEUE_CAPACITY 2u
#define PWT_BRIDGE_ACL_QUEUE_CAPACITY 4u
#define PWT_BRIDGE_RX_QUEUE_CAPACITY 4u

typedef struct {
    pwt_hci_status_t (*send)(void *context, const pwt_hci_packet_t *packet);
    pwt_hci_status_t (*receive)(void *context, pwt_hci_packet_t *packet);
    pwt_hci_status_t (*reset)(void *context);
} pwt_hci_link_ops_t;

typedef struct {
    uint32_t host_commands_enqueued;
    uint32_t host_acl_enqueued;
    uint32_t host_queue_busy;
    uint32_t controller_tx_sent;
    uint32_t controller_rx_received;
    uint32_t controller_waits;
    uint32_t flow_wait_cleared;
    uint32_t malformed_host_packets;
    uint32_t malformed_controller_packets;
    uint32_t release_mismatch;
    uint32_t resets;
    uint32_t recovery_failures;
    uint32_t packets_discarded_on_reset;
} pwt_controller_bridge_stats_t;

typedef struct {
    pwt_hci_packet_t packet;
} pwt_bridge_slot_t;

typedef enum {
    PWT_BRIDGE_WAIT_NONE = 0,
    PWT_BRIDGE_WAIT_COMMAND,
    PWT_BRIDGE_WAIT_ACL,
} pwt_bridge_wait_kind_t;

typedef struct {
    const pwt_hci_link_ops_t *link_ops;
    void *link_context;

    pwt_bridge_slot_t command_queue[PWT_BRIDGE_COMMAND_QUEUE_CAPACITY];
    uint8_t command_head;
    uint8_t command_tail;
    uint8_t command_count;

    pwt_bridge_slot_t acl_queue[PWT_BRIDGE_ACL_QUEUE_CAPACITY];
    uint8_t acl_head;
    uint8_t acl_tail;
    uint8_t acl_count;

    pwt_bridge_slot_t rx_queue[PWT_BRIDGE_RX_QUEUE_CAPACITY];
    uint8_t rx_head;
    uint8_t rx_tail;
    uint8_t rx_count;
    bool rx_leased;

    pwt_bridge_wait_kind_t wait_kind;
    uint16_t wait_identifier;
    bool faulted;

    pwt_controller_bridge_stats_t stats;
} pwt_controller_bridge_t;

void pwt_controller_bridge_init(
    pwt_controller_bridge_t *bridge,
    pwt_hci_transport_t *transport);

void pwt_controller_bridge_init_with_link(
    pwt_controller_bridge_t *bridge,
    const pwt_hci_link_ops_t *link_ops,
    void *link_context);

pwt_controller_backend_t pwt_controller_bridge_backend(
    pwt_controller_bridge_t *bridge);

void pwt_controller_bridge_service(pwt_controller_bridge_t *bridge);
pwt_hci_status_t pwt_controller_bridge_reset(pwt_controller_bridge_t *bridge);
bool pwt_controller_bridge_needs_recovery(const pwt_controller_bridge_t *bridge);

const pwt_controller_bridge_stats_t *pwt_controller_bridge_stats(
    const pwt_controller_bridge_t *bridge);

#endif
