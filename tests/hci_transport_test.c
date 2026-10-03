#include "picowutooth/hci_transport.h"

#include <stdio.h>
#include <string.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define TEST_CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct fake_frame {
    size_t length;
    uint8_t bytes[PWT_CYW43_FRAME_CAPACITY];
} fake_frame_t;

typedef struct fake_backend {
    int start_result;
    int write_result;
    int read_result;
    unsigned starts;
    unsigned stops;
    unsigned delays;
    uint8_t wlan_mac[PWT_BD_ADDR_LEN];
    fake_frame_t writes[16];
    size_t write_count;
    fake_frame_t reads[16];
    size_t read_count;
    size_t read_index;
} fake_backend_t;

static void set_header(fake_frame_t *frame, pwt_hci_packet_kind_t kind, size_t payload_length) {
    frame->length = payload_length + PWT_CYW43_HEADER_SIZE;
    frame->bytes[0] = (uint8_t)(payload_length & 0xffu);
    frame->bytes[1] = (uint8_t)((payload_length >> 8u) & 0xffu);
    frame->bytes[2] = (uint8_t)((payload_length >> 16u) & 0xffu);
    frame->bytes[3] = (uint8_t)kind;
}

static void queue_command_complete(fake_backend_t *fake, uint16_t opcode, uint8_t status) {
    fake_frame_t *frame = &fake->reads[fake->read_count++];
    const uint8_t event[] = {
        0x0eu, 0x04u, 0x01u,
        (uint8_t)(opcode & 0xffu), (uint8_t)(opcode >> 8u), status,
    };
    set_header(frame, PWT_HCI_PACKET_EVENT, sizeof(event));
    memcpy(&frame->bytes[PWT_CYW43_HEADER_SIZE], event, sizeof(event));
}

static void prepare_successful_init(fake_backend_t *fake) {
    memset(fake, 0, sizeof(*fake));
    const uint8_t mac[PWT_BD_ADDR_LEN] = {0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0xfeu};
    memcpy(fake->wlan_mac, mac, sizeof(mac));
    queue_command_complete(fake, 0x0c03u, 0u);
    queue_command_complete(fake, 0xfc01u, 0u);
}

static int fake_start(void *context) {
    fake_backend_t *fake = (fake_backend_t *)context;
    fake->starts++;
    return fake->start_result;
}

static void fake_stop(void *context) {
    fake_backend_t *fake = (fake_backend_t *)context;
    fake->stops++;
}

static int fake_write(void *context, uint8_t *frame, size_t length) {
    fake_backend_t *fake = (fake_backend_t *)context;
    if (fake->write_result != 0) {
        return fake->write_result;
    }
    if (fake->write_count >= ARRAY_LEN(fake->writes) || length > PWT_CYW43_FRAME_CAPACITY) {
        return -1;
    }
    fake_frame_t *written = &fake->writes[fake->write_count++];
    written->length = length;
    memcpy(written->bytes, frame, length);
    return 0;
}

static int fake_read(void *context, uint8_t *frame, size_t capacity, size_t *length) {
    fake_backend_t *fake = (fake_backend_t *)context;
    if (fake->read_result != 0) {
        return fake->read_result;
    }
    if (fake->read_index >= fake->read_count) {
        *length = 0u;
        return 0;
    }
    const fake_frame_t *queued = &fake->reads[fake->read_index++];
    if (queued->length > capacity) {
        *length = queued->length;
        return 0;
    }
    memcpy(frame, queued->bytes, queued->length);
    *length = queued->length;
    return 0;
}

static int fake_get_mac(void *context, uint8_t mac[PWT_BD_ADDR_LEN]) {
    fake_backend_t *fake = (fake_backend_t *)context;
    memcpy(mac, fake->wlan_mac, PWT_BD_ADDR_LEN);
    return 0;
}

static void fake_delay(void *context, uint32_t milliseconds) {
    fake_backend_t *fake = (fake_backend_t *)context;
    fake->delays += milliseconds;
}

static const pwt_hci_backend_ops_t fake_ops = {
    .start = fake_start,
    .stop = fake_stop,
    .write_raw = fake_write,
    .read_raw = fake_read,
    .get_wlan_mac = fake_get_mac,
    .delay_ms = fake_delay,
};

static int assert_frame(
    const fake_frame_t *frame,
    pwt_hci_packet_kind_t kind,
    const uint8_t *payload,
    size_t payload_length) {
    TEST_CHECK(frame->length == payload_length + PWT_CYW43_HEADER_SIZE);
    TEST_CHECK(frame->bytes[0] == (uint8_t)(payload_length & 0xffu));
    TEST_CHECK(frame->bytes[1] == (uint8_t)((payload_length >> 8u) & 0xffu));
    TEST_CHECK(frame->bytes[2] == (uint8_t)((payload_length >> 16u) & 0xffu));
    TEST_CHECK(frame->bytes[3] == (uint8_t)kind);
    TEST_CHECK(memcmp(&frame->bytes[PWT_CYW43_HEADER_SIZE], payload, payload_length) == 0);
    return 0;
}

static int test_initialization_and_identity(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);

    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_READY);
    TEST_CHECK(fake.starts == 1u);
    TEST_CHECK(fake.write_count == 2u);

    const uint8_t reset_command[] = {0x03u, 0x0cu, 0x00u};
    TEST_CHECK(assert_frame(&fake.writes[0], PWT_HCI_PACKET_COMMAND,
                            reset_command, sizeof(reset_command)) == 0);

    const uint8_t set_addr_command[] = {
        0x01u, 0xfcu, 0x06u,
        0xffu, 0x44u, 0x33u, 0x22u, 0x11u, 0x02u,
    };
    TEST_CHECK(assert_frame(&fake.writes[1], PWT_HCI_PACKET_COMMAND,
                            set_addr_command, sizeof(set_addr_command)) == 0);

    const uint8_t expected_addr[] = {0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0xffu};
    uint8_t actual_addr[PWT_BD_ADDR_LEN];
    TEST_CHECK(pwt_hci_transport_bd_addr(&transport, actual_addr) == PWT_HCI_OK);
    TEST_CHECK(memcmp(actual_addr, expected_addr, sizeof(expected_addr)) == 0);

    pwt_hci_transport_deinit(&transport);
    TEST_CHECK(fake.stops == 1u);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_DOWN);
    return 0;
}

static int test_tx_framing_and_ownership(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);

    pwt_hci_packet_t packet = {
        .kind = PWT_HCI_PACKET_ACL,
        .length = 8u,
        .payload = {0x04u, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u},
    };
    const uint8_t expected[] = {0x04u, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u};
    TEST_CHECK(pwt_hci_transport_send(&transport, &packet) == PWT_HCI_OK);
    TEST_CHECK(fake.write_count == 3u);
    TEST_CHECK(assert_frame(&fake.writes[2], PWT_HCI_PACKET_ACL,
                            expected, sizeof(expected)) == 0);

    memset(packet.payload, 0xa5, packet.length);
    TEST_CHECK(memcmp(&fake.writes[2].bytes[PWT_CYW43_HEADER_SIZE],
                      expected, sizeof(expected)) == 0);

    packet.kind = PWT_HCI_PACKET_EVENT;
    TEST_CHECK(pwt_hci_transport_send(&transport, &packet) == PWT_HCI_E_DIRECTION);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_READY);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

static int test_rx_framing_exact_payload_and_no_double_strip(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);

    fake_frame_t *incoming = &fake.reads[fake.read_count++];
    const uint8_t payload[] = {0x04u, 0x00u, 0x00u, 0x04u, 0xdeu, 0xadu, 0xbeu, 0xefu};
    set_header(incoming, PWT_HCI_PACKET_EVENT, sizeof(payload));
    memcpy(&incoming->bytes[PWT_CYW43_HEADER_SIZE], payload, sizeof(payload));

    pwt_hci_packet_t packet;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_OK);
    TEST_CHECK(packet.kind == PWT_HCI_PACKET_EVENT);
    TEST_CHECK(packet.length == sizeof(payload));
    TEST_CHECK(memcmp(packet.payload, payload, sizeof(payload)) == 0);

    memset(incoming->bytes, 0x33, incoming->length);
    TEST_CHECK(memcmp(packet.payload, payload, sizeof(payload)) == 0);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

static int test_malformed_frames_enter_error(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);

    fake_frame_t *short_frame = &fake.reads[fake.read_count++];
    short_frame->length = 3u;
    memset(short_frame->bytes, 0, short_frame->length);
    pwt_hci_packet_t packet;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_E_MALFORMED);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_ERROR);
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_E_NOT_READY);

    pwt_hci_transport_deinit(&transport);

    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);
    fake_frame_t *bad_length = &fake.reads[fake.read_count++];
    set_header(bad_length, PWT_HCI_PACKET_EVENT, 2u);
    bad_length->bytes[PWT_CYW43_HEADER_SIZE] = 0xaau;
    bad_length->bytes[PWT_CYW43_HEADER_SIZE + 1u] = 0xbbu;
    bad_length->bytes[0] = 3u;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_E_MALFORMED);
    pwt_hci_transport_deinit(&transport);

    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);
    fake_frame_t *bad_kind = &fake.reads[fake.read_count++];
    set_header(bad_kind, PWT_HCI_PACKET_EVENT, 1u);
    bad_kind->bytes[3] = 0x7fu;
    bad_kind->bytes[4] = 0x00u;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_E_MALFORMED);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

static int test_oversized_reported_frame(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);

    fake_frame_t *oversized = &fake.reads[fake.read_count++];
    oversized->length = PWT_CYW43_FRAME_CAPACITY + 1u;
    pwt_hci_packet_t packet;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_E_OVERSIZE);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_ERROR);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

static int test_transport_error_and_reset_recovery(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);

    fake.write_result = -9;
    pwt_hci_packet_t packet = {
        .kind = PWT_HCI_PACKET_COMMAND,
        .length = 3u,
        .payload = {0x03u, 0x0cu, 0x00u},
    };
    TEST_CHECK(pwt_hci_transport_send(&transport, &packet) == PWT_HCI_E_TRANSPORT);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_ERROR);
    TEST_CHECK(pwt_hci_transport_send(&transport, &packet) == PWT_HCI_E_NOT_READY);

    fake.write_result = 0;
    fake.read_count = 0u;
    fake.read_index = 0u;
    queue_command_complete(&fake, 0x0c03u, 0u);
    queue_command_complete(&fake, 0xfc01u, 0u);
    TEST_CHECK(pwt_hci_transport_reset(&transport) == PWT_HCI_OK);
    TEST_CHECK(fake.stops == 1u);
    TEST_CHECK(fake.starts == 2u);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_READY);

    pwt_hci_transport_deinit(&transport);
    TEST_CHECK(fake.stops == 2u);
    return 0;
}

static int test_controller_reject_and_timeout(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    fake.read_count = 0u;
    queue_command_complete(&fake, 0x0c03u, 1u);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_E_CONTROLLER);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_ERROR);
    pwt_hci_transport_deinit(&transport);

    memset(&fake, 0, sizeof(fake));
    const uint8_t mac[PWT_BD_ADDR_LEN] = {0x02u, 0u, 0u, 0u, 0u, 1u};
    memcpy(fake.wlan_mac, mac, sizeof(mac));
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_E_TIMEOUT);
    TEST_CHECK(fake.delays == 1000u);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

static int test_empty_receive(void) {
    fake_backend_t fake;
    pwt_hci_transport_t transport;
    prepare_successful_init(&fake);
    TEST_CHECK(pwt_hci_transport_init(&transport, &fake_ops, &fake) == PWT_HCI_OK);
    pwt_hci_packet_t packet;
    TEST_CHECK(pwt_hci_transport_receive(&transport, &packet) == PWT_HCI_NO_DATA);
    TEST_CHECK(pwt_hci_transport_state(&transport) == PWT_HCI_STATE_READY);
    pwt_hci_transport_deinit(&transport);
    return 0;
}

int main(void) {
    int result = 0;
    result |= test_initialization_and_identity();
    result |= test_tx_framing_and_ownership();
    result |= test_rx_framing_exact_payload_and_no_double_strip();
    result |= test_malformed_frames_enter_error();
    result |= test_oversized_reported_frame();
    result |= test_transport_error_and_reset_recovery();
    result |= test_controller_reject_and_timeout();
    result |= test_empty_receive();
    if (result == 0) {
        puts("PWT-002 HCI transport tests passed");
    }
    return result;
}
