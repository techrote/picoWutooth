# picoWutooth programme

## Goal

Produce firmware for Raspberry Pi Pico W that presents the board to a host as a standards-conforming USB Bluetooth HCI controller, forwarding Bluetooth controller traffic to/from the on-board CYW43439.

Primary acceptance host: Windows 11 using Microsoft's in-box USB Bluetooth stack. Linux is the first diagnostic host because its USB/Bluetooth tooling exposes transport behaviour clearly.

## Initial plan

The first-pass plan was:

1. create a Pico SDK + TinyUSB firmware project;
2. expose TinyUSB's Bluetooth HCI device class;
3. connect USB HCI callbacks directly to CYW43 HCI read/write calls;
4. test enumeration on Windows;
5. add BLE and Classic acceptance;
6. harden and release.

That path is plausible but couples too many unknowns at once.

## Review of the initial plan

The review identified the following failure modes.

### Coupled bring-up

A direct "wire it together then plug it into Windows" approach makes four independent classes of failure look identical:

- CYW43439 firmware/controller initialization;
- CYW43-specific HCI framing;
- TinyUSB descriptor/endpoint mistakes;
- host driver binding or host-stack policy.

These must be isolated before integration.

### Controller initialization is not a host-driver responsibility

Windows can bind a standards-shaped USB Bluetooth controller to `BthUsb.sys`, but the generic host driver should not be assumed to perform CYW43439 vendor firmware/bootstrap work. picoWutooth must establish a controller-ready state itself before relying on host HCI commands.

### USB composite debugging is risky during first binding

Adding CDC or a vendor interface from the outset may alter descriptor topology and driver matching. MVP firmware should enumerate only as Bluetooth HCI. Debugging should use compile-time logging, UART/SWD, or host-visible evidence that does not change the production USB identity.

### HCI framing differs on each side

USB transports commands/events/ACL through distinct transfer types/endpoints; CYW43's low-level transport expects a four-byte packet pre-header with HCI packet type carried in the fourth byte. Treating the bridge as a raw byte pipe would be incorrect.

### Backpressure is correctness

RP2040↔CYW43 SPI scheduling and USB endpoint completion are asynchronous. Fixed buffers without explicit ownership/backpressure can create intermittent packet loss that looks like radio instability. Buffer state machines therefore need native deterministic tests before hardware acceptance.

### Hardware debugging should progress from observable to opaque hosts

Linux provides `btmon`, `bluetoothctl`, sysfs and standard USB inspection tools, so it is a better first physical-host acceptance environment. Windows 11 remains the primary product target after the transport is already known-good.

### Audio should not distort the MVP

TinyUSB supports Bluetooth HCI and optional ISO endpoint alternatives, while Windows supports in-band SCO on USB Bluetooth controllers. However, voice/audio introduces isochronous scheduling and substantially tighter timing. Ordinary BLE/HID/GATT dongle functionality does not require this, so SCO/ISO is deferred.

## Improved plan

The revised campaign separates independent proofs and makes each integration step preserve evidence from the layer below.

### Phase A — deterministic foundation

**PWT-001** establishes the Pico SDK project, pinned dependencies, CI cross-build, native/unit-test harness, formatting/static checks where useful, generated UF2 artifact and documentation skeleton.

Gate: a clean checkout builds deterministically in CI without hardware.

### Phase B — prove each half independently

**PWT-002** implements the CYW43 HCI-side adapter and documents the exact supported initialization boundary. It must isolate CYW43's four-byte transport framing, controller readiness and reset/error behaviour behind a narrow internal interface. Mock/native tests validate framing and ownership; hardware is not claimed merely from compilation.

**PWT-003** implements the TinyUSB Bluetooth HCI USB identity against a deterministic stub/synthetic controller. It validates descriptors, endpoint roles and callback paths independently of CYW43.

PWT-002 and PWT-003 may proceed in parallel after PWT-001.

### Phase C — integrate the transport

**PWT-004** joins the two proven halves. It owns queueing, packet ownership, backpressure, ordering, reset/re-enumeration behaviour, bounds checks and instrumentation that does not change production USB identity.

Gate: native adversarial tests plus Pico W cross-build pass; no known silent-drop path remains.

### Phase D — physical protocol acceptance

**PWT-005** performs Linux hardware acceptance on an exact PWT-004 firmware commit. Required milestones include USB enumeration, Bluetooth driver binding, HCI reset/controller information, BLE scan, at least one controlled BLE connection and packet evidence sufficient to localize failures.

This is the first issue allowed to claim real RF/controller behaviour.

### Phase E — primary host acceptance

**PWT-006** validates Windows 11 binding to the in-box Bluetooth USB stack, Generic Bluetooth Adapter/controller visibility, BLE discovery and connection, replug/reboot/device-disable recovery, and absence of a custom host driver requirement.

Gate: evidence is tied to exact firmware and Windows build.

### Phase F — ordinary dongle breadth and robustness

**PWT-007** adds/validates BR/EDR interoperability where supported, representative HID or other Classic traffic, repeated connect/disconnect, malformed/boundary traffic tests, host reset, USB re-enumeration and sustained ACL load within Full-Speed USB constraints.

### Phase G — release

**PWT-008** produces v0.1: reproducible UF2, dependency provenance/licenses, flashing/use documentation, known-limitations matrix, evidence index and tagged release criteria.

### Post-MVP

**PWT-009** is a separate feasibility campaign for SCO/ISO audio transport. It must not block v0.1 and must not destabilize the proven non-audio endpoint topology.

## Dependency graph

```text
PWT-001
  ├── PWT-002 ─┐
  └── PWT-003 ─┴─> PWT-004 -> PWT-005 -> PWT-006 -> PWT-007 -> PWT-008
                                                            \
                                                             -> PWT-009 (post-MVP only)
```

PWT-009 is logically downstream of the stable bridge/host acceptance work but is not a v0.1 dependency.

## MVP completion definition

v0.1 is complete only when:

- the repository builds from a clean checkout using pinned/documented dependencies;
- automated framing/buffer/descriptor tests pass;
- a Pico W enumerates as the intended Bluetooth USB class device;
- Linux can drive the controller through standard Bluetooth tooling and complete BLE discovery/connection;
- Windows 11 binds without a project-specific host driver and completes equivalent BLE acceptance;
- representative BR/EDR functionality is either accepted or precisely documented as a controller/firmware limitation;
- repeated reset/replug/connect/disconnect does not expose known packet-loss or deadlock defects;
- the release UF2 is reproducible and traceable to source, dependency versions and evidence.

## Non-goals for v0.1

- implementing a Bluetooth host/profile stack on RP2040;
- Wi-Fi/Bluetooth coexistence;
- a USB CDC/vendor debug interface in the production descriptor set;
- SCO/ISO audio;
- throughput benchmarking beyond proving the bridge does not become an avoidable bottleneck for ordinary Bluetooth traffic;
- custom Windows drivers.
