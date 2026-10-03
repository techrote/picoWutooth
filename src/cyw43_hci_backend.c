#include "picowutooth/cyw43_hci_backend.h"

#include <stdint.h>
#include <string.h>

#include "cyw43.h"
#include "pico/async_context.h"
#include "pico/cyw43_driver.h"
#include "pico/time.h"

static void pwt_cyw43_stop(void *context);

static int pwt_cyw43_start(void *context) {
    pwt_cyw43_hci_backend_t *backend = (pwt_cyw43_hci_backend_t *)context;
    if (backend == NULL) {
        return -1;
    }

    if (!async_context_threadsafe_background_init_with_defaults(&backend->async_context)) {
        return -1;
    }
    backend->context_initialized = true;

    if (!cyw43_driver_init(&backend->async_context.core)) {
        pwt_cyw43_stop(backend);
        return -1;
    }
    backend->driver_initialized = true;

    const int result = cyw43_bluetooth_hci_init();
    if (result != 0) {
        pwt_cyw43_stop(backend);
        return result;
    }
    return 0;
}

static void pwt_cyw43_stop(void *context) {
    pwt_cyw43_hci_backend_t *backend = (pwt_cyw43_hci_backend_t *)context;
    if (backend == NULL) {
        return;
    }
    if (backend->driver_initialized) {
        cyw43_driver_deinit(&backend->async_context.core);
        backend->driver_initialized = false;
    }
    if (backend->context_initialized) {
        async_context_deinit(&backend->async_context.core);
        backend->context_initialized = false;
    }
}

static int pwt_cyw43_write_raw(void *context, uint8_t *frame, size_t length) {
    (void)context;
    return cyw43_bluetooth_hci_write(frame, length);
}

static int pwt_cyw43_read_raw(
    void *context,
    uint8_t *frame,
    size_t capacity,
    size_t *length) {
    (void)context;
    if (capacity > UINT32_MAX || length == NULL) {
        return -1;
    }
    uint32_t cyw43_length = 0u;
    const int result = cyw43_bluetooth_hci_read(frame, (uint32_t)capacity, &cyw43_length);
    *length = (size_t)cyw43_length;
    return result;
}

static int pwt_cyw43_get_wlan_mac(void *context, uint8_t mac[PWT_BD_ADDR_LEN]) {
    (void)context;
    return cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
}

static void pwt_cyw43_delay_ms(void *context, uint32_t milliseconds) {
    (void)context;
    sleep_ms(milliseconds);
}

static const pwt_hci_backend_ops_t pwt_cyw43_ops = {
    .start = pwt_cyw43_start,
    .stop = pwt_cyw43_stop,
    .write_raw = pwt_cyw43_write_raw,
    .read_raw = pwt_cyw43_read_raw,
    .get_wlan_mac = pwt_cyw43_get_wlan_mac,
    .delay_ms = pwt_cyw43_delay_ms,
};

void pwt_cyw43_hci_backend_init(pwt_cyw43_hci_backend_t *backend) {
    if (backend != NULL) {
        memset(backend, 0, sizeof(*backend));
    }
}

const pwt_hci_backend_ops_t *pwt_cyw43_hci_backend_ops(void) {
    return &pwt_cyw43_ops;
}
