#ifndef PWT_USB_HCI_APP_H
#define PWT_USB_HCI_APP_H

#include <stdbool.h>

#include "picowutooth/hci_transport.h"

bool pwt_usb_hci_app_init(pwt_hci_transport_t *transport);
void pwt_usb_hci_app_task(void);

#endif
