# Primary technical references

Use primary upstream documentation/source wherever possible. Re-check live upstream state when an issue depends on an API detail, then pin the dependency revision used by picoWutooth.

## Raspberry Pi / Pico SDK

- Pico SDK networking/Bluetooth libraries:  
  https://www.raspberrypi.com/documentation/pico-sdk/networking.html
- Pico SDK repository:  
  https://github.com/raspberrypi/pico-sdk
- CYW43 BTstack HCI transport source, including the CYW43 four-byte packet pre-header contract:  
  https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_cyw43_driver/btstack_hci_transport_cyw43.c
- Pico CYW43 driver build integration:  
  https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_cyw43_driver/CMakeLists.txt
- Pico SDK releases:  
  https://github.com/raspberrypi/pico-sdk/releases

Important current upstream facts to verify against the pinned revision:

- `pico_btstack_cyw43` provides low-level Bluetooth HCI support/integration for CYW43.
- the CYW43 HCI transport uses a custom four-byte prefix and stores HCI packet type in byte 3;
- Pico W routes Bluetooth and Wi-Fi traffic to CYW43439 over the board's internal transport, so Bluetooth-only MVP operation should be established before coexistence work.

Do not assume a `master` source line remains unchanged; record the pinned SDK revision in the build.

## TinyUSB

- TinyUSB repository:  
  https://github.com/hathach/tinyusb
- TinyUSB documentation:  
  https://docs.tinyusb.org/
- integration guide:  
  https://docs.tinyusb.org/en/latest/integration.html
- device descriptor helpers, including Bluetooth HCI descriptor macros:  
  https://github.com/hathach/tinyusb/blob/master/src/device/usbd.h
- Bluetooth HCI device class header:  
  https://github.com/hathach/tinyusb/blob/master/src/class/bth/bth_device.h
- TinyUSB configuration options:  
  https://github.com/hathach/tinyusb/blob/master/src/tusb_option.h
- releases:  
  https://github.com/hathach/tinyusb/releases

Important current upstream facts to verify against the pinned revision:

- TinyUSB device stack supports Bluetooth Host Controller Interface (BTH HCI);
- descriptor helpers identify the primary controller as Wireless Controller class `0xE0`, subclass `0x01`, protocol `0x01`;
- `CFG_TUD_BTH` enables the class and current upstream requires `CFG_TUD_BTH_ISO_ALT_COUNT` to be defined when it is enabled;
- ISO alternatives are not required for the MVP and should remain absent/zero unless a later audio issue deliberately changes that;
- consult the pinned release notes for Bluetooth-HCI bug fixes such as ACL IN zero-length-packet handling.

A historical TinyUSB/Mynewt example demonstrates the intended model: enumerate the MCU as a Bluetooth controller and let a host OS scan/connect through its native stack:
https://github.com/hathach/mynewt-tinyusb-example

## Microsoft Windows

- USB device class drivers included in Windows:  
  https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/supported-usb-classes
- Bluetooth host radio support:  
  https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-host-radio-support
- Bluetooth platform/component guidance:  
  https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/bluetooth

Key Windows contract:

- Wireless Controller class `E0h`, subclass `01h`, protocol `01h` is supported by Microsoft's `BthUsb.sys` on Windows 10/11;
- USB Bluetooth controllers can use the in-box Bluetooth USB transport;
- USB transport supports in-band SCO, but SCO/ISO is deliberately post-MVP here;
- do not make a custom Windows driver part of the MVP unless evidence proves the standard class path cannot satisfy the project.

## Bluetooth USB/HCI specifications

When exact descriptor, endpoint, packet or command behaviour is disputed, consult the Bluetooth Core Specification and Bluetooth USB transport/class material from the Bluetooth SIG/USB-IF applicable to the controller/host versions being tested. Do not rely on blog posts for normative packet layout.

## Reference policy

Implementation PRs should link the exact upstream source/docs that justify non-obvious protocol choices. If a workaround depends on an upstream bug, record:

- dependency revision;
- upstream issue/commit if available;
- local workaround;
- condition for removing it.

Avoid copying large upstream source fragments into the repository when a narrow adapter around a supported API is sufficient.
