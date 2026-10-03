#ifndef PICOWUTOOTH_CYW43_HCI_BACKEND_H
#define PICOWUTOOTH_CYW43_HCI_BACKEND_H

#include <stdbool.h>

#include "pico/async_context_threadsafe_background.h"
#include "picowutooth/hci_transport.h"

typedef struct pwt_cyw43_hci_backend {
    async_context_threadsafe_background_t async_context;
    bool context_initialized;
    bool driver_initialized;
} pwt_cyw43_hci_backend_t;

void pwt_cyw43_hci_backend_init(pwt_cyw43_hci_backend_t *backend);
const pwt_hci_backend_ops_t *pwt_cyw43_hci_backend_ops(void);

#endif
