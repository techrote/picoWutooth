# Architecture

## System boundary

picoWutooth is a Bluetooth **controller transport adapter**.

The host computer owns:

- HCI host state;
- L2CAP;
- ATT/GATT;
- SMP/security policy;
- SDP;
- HID/profile implementations;
- pairing UI and persistent host-side device state.

The Pico W owns:

- CYW43439 initialization required before normal HCI operation;
- translation between USB Bluetooth HCI transfers and CYW43's transport representation;
- bounded buffering, ordering and backpressure;
- USB device lifecycle;
- controller reset/recovery coordination;
- minimal diagnostics that do not alter the production USB identity.

It must not grow a second Bluetooth host stack merely to make the bridge work.

## Data path

```text
Host OS Bluetooth stack
        |
        | USB Bluetooth HCI class
        |  control OUT : HCI commands
        |  interrupt IN: HCI events
        |  bulk OUT    : ACL host -> controller
        |  bulk IN     : ACL controller -> host
        |  ISO         : deferred post-MVP
        v
+-------------------------------+
| RP2040 / TinyUSB              |
|                               |
| USB class adapter             |
|      |                        |
| packet ownership / queues     |
|      |                        |
| CYW43 HCI adapter             |
+---------------+---------------+
                |
                | Pico W internal CYW43 transport
                | custom 4-byte pre-header
                v
          CYW43439 controller
                |
                v
              2.4 GHz
```

## USB identity

MVP production firmware is a single-purpose USB Bluetooth HCI device.

Required class identity is the standard Bluetooth wireless-controller shape used by the in-box Windows Bluetooth USB driver: Wireless Controller class `0xE0`, subclass `0x01`, protocol `0x01`.

TinyUSB already contains Bluetooth HCI device-class support and descriptor helpers. Use upstream TinyUSB facilities rather than reimplementing a class driver unless a proven blocker requires otherwise.

MVP endpoint roles:

| Traffic | USB mechanism |
|---|---|
| HCI command host→controller | class control request / control OUT path |
| HCI event controller→host | interrupt IN |
| ACL host→controller | bulk OUT |
| ACL controller→host | bulk IN |
| SCO/ISO | excluded from MVP |

Do not add CDC, HID, vendor-specific or other production interfaces during MVP. If debug USB is useful during development, use a separate compile-time diagnostic build and prove that it cannot be confused with acceptance firmware.

## CYW43 transport boundary

The Pico SDK exposes low-level Bluetooth HCI support for CYW43. Its upstream CYW43 HCI transport documents a mandatory four-byte packet header in front of the actual HCI packet, with HCI packet type stored in the fourth byte.

Therefore the internal bridge API must represent **packet type + payload**, not an untyped stream.

Conceptual internal type:

```c
enum hci_packet_kind {
    HCI_COMMAND,
    HCI_EVENT,
    HCI_ACL,
    HCI_ISO,   // reserved; post-MVP
};

struct hci_packet {
    enum hci_packet_kind kind;
    uint16_t length;
    uint8_t payload[MAX_PACKET];
};
```

The real implementation may use spans/ring slots rather than copying this structure, but the semantic separation must remain.

### Translation rule

- USB-facing code must not expose or expect the CYW43 four-byte pre-header.
- CYW43-facing code must add/remove the pre-header exactly once.
- Packet kind is derived from the USB transfer path on host→controller traffic and from CYW43 packet metadata on controller→host traffic.
- Tests must detect double-prefixing, missing prefix, wrong packet type and length mismatch.

## Controller initialization

The controller must reach a defined `READY` state before normal bridged host traffic is accepted.

The implementation must identify and document the supported Pico SDK initialization boundary rather than cargo-culting private internals. Preferred order:

1. use public Pico SDK/CYW43 initialization facilities;
2. use an upstream-exposed HCI transport abstraction if it permits a host-stack-free controller path;
3. only use lower-level CYW43 functions when necessary and wrap them locally;
4. do not fork/copy the CYW43 driver merely to bypass a small adapter problem.

The exact initialization sequence is owned by PWT-002 and becomes an architectural contract once proven.

Windows `BthUsb.sys` must not be expected to perform Pico-specific or Broadcom-specific firmware bootstrap.

## State model

Minimum firmware lifecycle:

```text
BOOT
  -> CYW_INIT
  -> CONTROLLER_READY
  -> USB_READY
  -> RUNNING
       |   ^
       v   |
     RECOVER
       |
       +-> CONTROLLER_READY / USB_READY
       +-> FATAL (bounded, observable failure)
```

Whether USB is physically initialized before controller readiness is an implementation detail, but host-visible HCI operations must not race an uninitialized controller.

Reset handling must define the relationship among:

- host HCI Reset command;
- USB bus reset;
- USB disconnect/reconnect;
- CYW43 transport failure;
- firmware watchdog/recovery.

Do not silently reboot the Pico for ordinary recoverable transport errors unless evidence shows that controller-level recovery is unsafe.

## Buffering and ownership

The bridge is asynchronous in both directions. Correctness requirements:

- bounded static memory;
- explicit buffer ownership;
- no use-after-submit of TinyUSB transfer buffers;
- no overwrite of a CYW43 TX packet before transport ownership returns;
- preserved packet ordering within each HCI traffic class;
- explicit handling when destination capacity is unavailable;
- no silent packet drops;
- counters for queue-full, malformed packet, retry/recovery and reset paths.

Backpressure policy must be testable without physical radio hardware.

Prefer queues of complete HCI packets or length-delimited ring slots over arbitrary byte FIFOs that can lose framing after one corruption.

## Concurrency

Start with the simplest scheduler compatible with the Pico SDK/CYW43 integration and TinyUSB device tasking. Do not introduce FreeRTOS or multicore merely for throughput before measurements prove the need.

Interrupt handlers should do bounded work and defer packet processing to normal task context where practical.

Any cross-context queue must document:

- producer;
- consumer;
- ownership transition;
- interrupt/thread safety primitive;
- maximum capacity;
- overflow behaviour.

## Bluetooth address and controller identity

PWT-002/PWT-005 must verify how the selected initialization path obtains or establishes a unique Bluetooth address and controller identity. Do not hard-code a globally reused BD_ADDR.

Record HCI `Read Local Version Information`, supported commands/features, buffer sizes and BD_ADDR during physical acceptance so future regressions can distinguish firmware/controller changes from bridge changes.

## Full-Speed USB constraints

RP2040 USB device operation is Full Speed. That is sufficient for the MVP's ordinary HCI command/event/ACL traffic, but performance must be measured rather than assumed for audio or unusually heavy traffic.

Do not add complexity such as multicore queueing or custom USB class code merely to optimize an unmeasured path.

## Deferred architecture

### Wi-Fi coexistence

Pico W Wi-Fi and Bluetooth share CYW43/internal transport resources. Wi-Fi coexistence is excluded until Bluetooth-only operation is stable.

### Composite USB diagnostics

Deferred until after Windows generic Bluetooth binding is proven. A later design may add a separate diagnostic firmware image rather than changing the production descriptor.

### SCO/ISO audio

Post-MVP. TinyUSB exposes optional Bluetooth ISO endpoint alternatives, and Windows supports in-band SCO for USB Bluetooth controllers, but isochronous scheduling deserves a separate campaign.
