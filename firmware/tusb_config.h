#ifndef PWT_TUSB_CONFIG_H
#define PWT_TUSB_CONFIG_H

#include "usb_descriptor_data.h"

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#if BOARD_TUD_RHPORT == 0
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | BOARD_TUD_MAX_SPEED)
#elif BOARD_TUD_RHPORT == 1
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_DEVICE | BOARD_TUD_MAX_SPEED)
#else
#error Unsupported TinyUSB device root-hub port
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined by the Pico SDK TinyUSB integration
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED BOARD_TUD_MAX_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_BTH 1
#define CFG_TUD_BTH_ISO_ALT_COUNT 0
#define CFG_TUD_BTH_EVENT_EPSIZE PWT_USB_EVENT_EP_SIZE
#define CFG_TUD_BTH_DATA_EPSIZE PWT_USB_ACL_EP_SIZE
#define CFG_TUD_BTH_HISTORICAL_COMPATIBLE 0

#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#endif
