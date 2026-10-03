# Primary technical references

Use primary upstream documentation/source wherever possible. Re-check live upstream state when an issue depends on an API detail, then pin the dependency revision used by picoWutooth.

## Pinned foundation baseline

PWT-001 pins the dependency set used by the project foundation:

| Dependency | Pinned revision |
|---|---|
| Raspberry Pi Pico SDK | release `2.3.1`, commit `079c6f39023649b154152db30f1d781e884879bc` |
| TinyUSB (SDK submodule) | `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` |
| CYW43 driver (SDK submodule) | `055d64274b014dd7b1c2fc94d26e8a18face7124` |
| BTstack (SDK submodule) | `eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec` |
| GitHub `actions/checkout` | `3d3c42e5aac5ba805825da76410c181273ba90b1` (v7.0.1) |
| GitHub `actions/upload-artifact` | `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a` (v7.0.1) |

The SDK release was selected as the current stable Raspberry Pi release at foundation time (published 2026-09-04). The repository helper scripts verify the SDK and the three Bluetooth-relevant submodule SHAs after checkout instead of trusting a floating tag or branch.

CI uses `ubuntu-24.04`, CMake/Ninja, and Ubuntu's `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, and `libstdc++-arm-none-eabi-newlib` packages. Exact compiler and CMake version strings are recorded in each firmware build manifest. The source tree does not vendor or maintain an independent TinyUSB checkout.

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

## PWT-002 pinned CYW43 controller contract

PWT-002 was implemented against the exact foundation revisions above. The
non-obvious controller-only decisions are grounded in these pinned source files:

- Pico SDK `pico/cyw43_driver.h` and `cyw43_driver.c` at
  `079c6f39023649b154152db30f1d781e884879bc`: public async-context
  `cyw43_driver_init/deinit`, OTP/board-unique WLAN MAC handling;
- Pico SDK `pico/cyw43_arch.h` / arch implementations at that revision: users
  may create their own async context and add CYW43 driver support directly;
- CYW43 driver `src/cyw43.h` and `src/cyw43_ctrl.c` at
  `055d64274b014dd7b1c2fc94d26e8a18face7124`:
  `cyw43_bluetooth_hci_init/read/write` are the public raw-HCI boundary and
  initialization loads the Bluetooth shared-bus firmware;
- Pico SDK `btstack_hci_transport_cyw43.c` at the pinned SDK revision: the
  transport reserves a four-byte CYW43 header, stores packet type in byte 3,
  derives Bluetooth identity from the WLAN MAC by incrementing octet 5, and
  programs the address for safety when OTP is absent;
- Pico SDK `btstack_chipset_cyw43.c` at the pinned SDK revision: the CYW43
  chipset Write_BD_ADDR command is vendor opcode `0xfc01` with six reversed
  address bytes;
- Pico SDK `cybt_shared_bus/cybt_shared_bus.c` at the pinned SDK revision:
  bytes 0..2 of the four-byte header are the little-endian HCI payload length
  and the shared-bus implementation uses packet types Command `0x01`, ACL
  `0x02`, Event `0x04`.

picoWutooth intentionally does not use `pico_btstack_cyw43` or
`btstack_hci_transport_cyw43_instance()` as its production boundary because
those facilities integrate BTstack host/run-loop ownership. picoWutooth needs
only the controller transport.

Known pinned-upstream caveat: `cybt_hci_write_buf()` can detect
`CYBT_ERR_QUEUE_FULL`, but `cyw43_btbus_write()` does not propagate that
return value and the public `cyw43_bluetooth_hci_write()` consequently cannot
report that specific internal queue-full condition. PWT-002 preserves the
supported public API instead of forking/copying private driver code; PWT-004 owns
bridge-level backpressure and PWT-005 provides physical stress evidence.

These source observations are API/implementation evidence, not physical
Bluetooth acceptance.

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

PWT-003 additionally carries `patches/tinyusb-bth-no-iso.patch` against exact TinyUSB commit `86ad6e56c1700e85f1c5678607a762cfe3aa2f47`. At this revision, `btd_open()` unconditionally walks a first ISO/voice interface and the class state contains an array sized by `CFG_TUD_BTH_ISO_ALT_COUNT`; that prevents a primary-controller-only descriptor from operating correctly with the required value `0`. The local patch compiles out only those voice fields and parsing paths when the count is zero. The SDK fetch helpers apply it idempotently after verifying the exact upstream commit, and the build manifest records the patch SHA-256.

The same pinned TinyUSB CMake aggregate does not include `src/class/bth/bth_device.c` in its device source list. With `CFG_TUD_BTH=1`, `usbd.c` registers the BTH driver but would otherwise leave its symbols unresolved. picoWutooth therefore adds that exact pinned upstream source to the firmware target explicitly; it is not vendored or reimplemented. Removal condition for both integration workarounds: adopt an upstream/Pico SDK combination that includes the BTH source and natively supports `CFG_TUD_BTH_ISO_ALT_COUNT=0`, then pass the same descriptor/routing and cross-build gates without the local accommodations.


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
