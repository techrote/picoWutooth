#ifndef PWT_USB_DESCRIPTOR_DATA_H
#define PWT_USB_DESCRIPTOR_DATA_H

#include <stddef.h>
#include <stdint.h>

#define PWT_USB_CLASS_WIRELESS_CONTROLLER 0xE0u
#define PWT_USB_BTH_SUBCLASS 0x01u
#define PWT_USB_BTH_PROTOCOL_PRIMARY_CONTROLLER 0x01u

#define PWT_USB_VID 0xCAFEu
#define PWT_USB_PID 0x4013u

#define PWT_USB_EP_EVENT_IN 0x81u
#define PWT_USB_EP_ACL_IN 0x82u
#define PWT_USB_EP_ACL_OUT 0x02u

#define PWT_USB_EVENT_EP_SIZE 16u
#define PWT_USB_ACL_EP_SIZE 64u

#define PWT_USB_DEVICE_DESCRIPTOR_LENGTH 18u
#define PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH 39u

extern const uint8_t pwt_usb_device_descriptor[PWT_USB_DEVICE_DESCRIPTOR_LENGTH];
extern const uint8_t pwt_usb_configuration_descriptor[PWT_USB_CONFIGURATION_DESCRIPTOR_LENGTH];

#endif
