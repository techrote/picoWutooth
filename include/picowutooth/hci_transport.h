#ifndef PICOWUTOOTH_HCI_TRANSPORT_H
#define PICOWUTOOTH_HCI_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWT_CYW43_HEADER_SIZE 4u
#define PWT_HCI_MAX_PAYLOAD 2048u
#define PWT_CYW43_FRAME_CAPACITY (PWT_CYW43_HEADER_SIZE + PWT_HCI_MAX_PAYLOAD)
#define PWT_BD_ADDR_LEN 6u

typedef enum pwt_hci_packet_kind {
    PWT_HCI_PACKET_COMMAND = 0x01,
    PWT_HCI_PACKET_ACL = 0x02,
    PWT_HCI_PACKET_EVENT = 0x04,
} pwt_hci_packet_kind_t;

typedef struct pwt_hci_packet {
    pwt_hci_packet_kind_t kind;
    uint16_t length;
    uint8_t payload[PWT_HCI_MAX_PAYLOAD];
} pwt_hci_packet_t;

typedef enum pwt_hci_status {
    PWT_HCI_OK = 0,
    PWT_HCI_NO_DATA = 1,
    PWT_HCI_E_ARGUMENT = -1,
    PWT_HCI_E_NOT_READY = -2,
    PWT_HCI_E_OVERSIZE = -3,
    PWT_HCI_E_MALFORMED = -4,
    PWT_HCI_E_DIRECTION = -5,
    PWT_HCI_E_TRANSPORT = -6,
    PWT_HCI_E_CONTROLLER = -7,
    PWT_HCI_E_TIMEOUT = -8,
} pwt_hci_status_t;

typedef enum pwt_hci_state {
    PWT_HCI_STATE_DOWN = 0,
    PWT_HCI_STATE_STARTING,
    PWT_HCI_STATE_READY,
    PWT_HCI_STATE_ERROR,
} pwt_hci_state_t;

typedef struct pwt_hci_backend_ops {
    int (*start)(void *context);
    void (*stop)(void *context);
    int (*write_raw)(void *context, uint8_t *frame, size_t length);
    int (*read_raw)(void *context, uint8_t *frame, size_t capacity, size_t *length);
    int (*get_wlan_mac)(void *context, uint8_t mac[PWT_BD_ADDR_LEN]);
    void (*delay_ms)(void *context, uint32_t milliseconds);
} pwt_hci_backend_ops_t;

typedef struct pwt_hci_transport {
    const pwt_hci_backend_ops_t *backend;
    void *backend_context;
    pwt_hci_state_t state;
    bool backend_started;
    uint8_t bd_addr[PWT_BD_ADDR_LEN];
    _Alignas(4) uint8_t tx_frame[PWT_CYW43_FRAME_CAPACITY];
    _Alignas(4) uint8_t rx_frame[PWT_CYW43_FRAME_CAPACITY];
} pwt_hci_transport_t;

pwt_hci_status_t pwt_hci_transport_init(
    pwt_hci_transport_t *transport,
    const pwt_hci_backend_ops_t *backend,
    void *backend_context);

pwt_hci_status_t pwt_hci_transport_send(
    pwt_hci_transport_t *transport,
    const pwt_hci_packet_t *packet);

pwt_hci_status_t pwt_hci_transport_receive(
    pwt_hci_transport_t *transport,
    pwt_hci_packet_t *packet);

pwt_hci_status_t pwt_hci_transport_reset(pwt_hci_transport_t *transport);
void pwt_hci_transport_deinit(pwt_hci_transport_t *transport);

pwt_hci_state_t pwt_hci_transport_state(const pwt_hci_transport_t *transport);
pwt_hci_status_t pwt_hci_transport_bd_addr(
    const pwt_hci_transport_t *transport,
    uint8_t bd_addr[PWT_BD_ADDR_LEN]);

#ifdef __cplusplus
}
#endif

#endif
