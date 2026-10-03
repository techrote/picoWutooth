# picoWutooth RAG index

This file is the compact retrieval entry point for project work. It does not replace the detailed documents; it tells an implementation agent what must be loaded before acting.

## Mandatory corpus

Read every item below before implementation:

| Path | Authority |
|---|---|
| `AGENTS.md` | repository-wide operating and merge rules |
| `docs/00-PROGRAMME.md` | goals, scope, plan review, revised dependency graph and completion definition |
| `docs/01-ARCHITECTURE.md` | technical architecture and invariants |
| `docs/02-VALIDATION.md` | CI, host and hardware acceptance matrix |
| `docs/03-EXECUTION-PROTOCOL.md` | issue/branch/PR/merge procedure and handoff rules |
| `docs/04-REFERENCES.md` | primary upstream specifications/source references |
| selected issue body + latest comments | bounded task and any live handoff |

If an issue changes an architectural contract, update the relevant authority document in the same PR.

## Product definition

picoWutooth aims to make a Raspberry Pi Pico W appear to a host computer as a normal USB Bluetooth HCI controller. The RP2040 terminates USB through TinyUSB and bridges standard USB HCI traffic to the on-board CYW43439 Bluetooth controller.

MVP priorities:

1. deterministic, reproducible firmware build and CI;
2. supported CYW43439 HCI startup path;
3. standards-shaped USB Bluetooth HCI descriptors/endpoints;
4. lossless HCI command/event and ACL bridging with explicit backpressure;
5. Linux diagnostic acceptance;
6. Windows 11 in-box `BthUsb.sys` acceptance;
7. BLE scan/connect;
8. BR/EDR basic interoperability and robustness;
9. reproducible UF2 release.

Deferred until after the MVP: Wi-Fi coexistence, composite USB debug interfaces, Bluetooth SCO/ISO audio and performance tuning not required for ordinary HID/GATT use.

## Key invariants

- The host owns the Bluetooth host stack. picoWutooth is a controller/transport bridge, not a parallel GATT/HID/profile implementation.
- USB HCI packet type is implied by USB transfer path; CYW43 transport uses its own four-byte pre-header with packet type in byte 3. Translation must be explicit and tested.
- CYW43439 must be completely initialized before ordinary host HCI traffic is allowed to depend on it; Windows' generic USB Bluetooth driver must not be expected to download Broadcom firmware.
- MVP USB identity is single-purpose Bluetooth HCI. Do not add CDC/vendor composite interfaces until generic binding is proven and preserved by tests.
- No silent packet drops. Queue capacity, backpressure, retry and reset behaviour are part of correctness.
- Hardware claims require captured evidence tied to an exact firmware commit.

## Programme work IDs

- **PWT-001 / #2** — reproducible firmware/build/CI foundation
- **PWT-002 / #3** — CYW43 raw-HCI adapter and initialization contract
- **PWT-003 / #4** — TinyUSB Bluetooth-HCI USB surface with deterministic stub/controller seam
- **PWT-004 / #5** — production HCI bridge, buffering and flow control
- **PWT-005 / #6** — Linux protocol/hardware acceptance
- **PWT-006 / #7** — Windows 11 generic Bluetooth adapter acceptance
- **PWT-007 / #8** — BR/EDR interoperability and disconnect/reset robustness
- **PWT-008 / #9** — v0.1 release hardening and reproducible UF2
- **PWT-009 / #10** — post-MVP SCO/ISO audio feasibility (non-blocking)

Programme tracker: **#1**.

See `docs/00-PROGRAMME.md` for dependencies and gates.
