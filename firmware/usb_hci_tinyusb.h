#ifndef PWT_USB_HCI_TINYUSB_H
#define PWT_USB_HCI_TINYUSB_H

#include "usb_hci_router.h"

void pwt_usb_hci_tinyusb_bind(pwt_usb_hci_router_t *router);
pwt_usb_tx_t pwt_usb_hci_tinyusb_transport(void);

#endif
