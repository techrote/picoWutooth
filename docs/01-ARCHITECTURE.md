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

### PWT-003 concrete USB contract

The PWT-003 production topology fixes the USB-facing contract to one primary Bluetooth HCI interface with no alternate settings:

- device and interface class/subclass/protocol: `0xE0 / 0x01 / 0x01`;
- HCI event endpoint: `0x81`, interrupt IN, 16-byte Full-Speed max packet;
- ACL controller→host endpoint: `0x82`, bulk IN, 64-byte Full-Speed max packet;
- ACL host→controller endpoint: `0x02`, bulk OUT, 64-byte Full-Speed max packet;
- no CDC, HID, vendor, IAD/composite companion, SCO/ISO interface or isochronous endpoint;
- `CFG_TUD_BTH_ISO_ALT_COUNT=0`.

The current engineering VID/PID is `0xCAFE:0x4013`. This is a provisional development identity, not a claim that picoWutooth owns an allocated USB vendor/product ID. Release hardening must replace it with an appropriately allocated identity without changing the single-purpose class topology.

The pinned TinyUSB BTH driver revision otherwise assumes at least one ISO/voice interface in `btd_open()` even when `CFG_TUD_BTH_ISO_ALT_COUNT` is zero. picoWutooth therefore applies the narrow, revision-locked patch `patches/tinyusb-bth-no-iso.patch`, which gates only the voice-interface storage/parsing when the ISO alternative count is zero. The primary TinyUSB BTH class driver and command/event/ACL APIs remain upstream. Remove the patch once the pinned/upgraded TinyUSB revision natively supports a zero-ISO primary controller and the descriptor tests remain green.

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

PWT-002 bounds one typed HCI packet to 2048 payload bytes, plus the private
four-byte CYW43 pre-header. This deliberately exceeds the expected CYW43439 ACL
frame size while remaining well below the pinned shared-bus 4096-byte ring. The
physical controller's reported buffer sizes remain an acceptance datum, not an
assumption baked into USB behavior.

## Controller initialization

PWT-002 selects a controller-only boundary from the pinned Pico SDK instead of
linking the SDK's BTstack HCI transport into production firmware. The firmware
creates an `async_context_threadsafe_background_t`, attaches the public
`pico_cyw43_driver` integration with `cyw43_driver_init()`, then uses the
CYW43 driver's public `cyw43_bluetooth_hci_init/read/write` functions. Wi-Fi
and lwIP remain disabled.

The CYW43 driver also expects the application to provide
`cyw43_bluetooth_hci_process()` when Bluetooth is compiled in. The SDK's
BTstack transport normally supplies that notification hook; picoWutooth supplies
its own minimal pull-transport hook instead, so satisfying the driver callback
does not import BTstack host/run-loop ownership.

This deliberately does **not** call `cyw43_arch_init()` with Bluetooth enabled:
in the pinned SDK that arch helper conditionally installs
`btstack_cyw43_init()`, which would add BTstack host/run-loop ownership that
picoWutooth does not need. The Pico SDK explicitly permits applications to
create their own async context and add CYW43 driver support directly.

The controller startup contract is:

1. initialize the background async context;
2. call `cyw43_driver_init()`;
3. call `cyw43_bluetooth_hci_init()`, which loads the CYW43439 Bluetooth
   firmware and initializes the shared Bluetooth bus;
4. obtain the board WLAN MAC through `cyw43_wifi_get_mac()`;
5. derive the Bluetooth public address exactly as the pinned SDK transport does:
   copy the WLAN address and increment octet 5;
6. issue HCI Reset (`0x0c03`) and require a successful Command Complete;
7. issue the CYW43/Broadcom Write_BD_ADDR vendor command (`0xfc01`) with the
   derived address and require a successful Command Complete;
8. only then expose `CONTROLLER_READY`.

The address source is not project-global. The CYW43 driver uses the device OTP
MAC when present and, if OTP lacks a MAC, its pinned Pico integration derives a
locally administered unicast MAC from the Pico unique board ID. This avoids the
controller firmware's documented fixed fallback address when OTP is absent.

Bootstrap Command Complete waits are bounded to 1000 one-millisecond attempts.
Initialization, framing or backend failures put the adapter in `ERROR`; normal
TX/RX is rejected until the adapter is fully reset or deinitialized. Adapter
reset tears down the CYW43 driver and async context and then repeats the complete
startup contract. Deinitialization removes the CYW43 driver from the async
context before destroying that context.

A later host HCI Reset is ordinary bridged HCI traffic; it is distinct from the
adapter's full transport-recovery reset.

Compilation proves only that this lifecycle matches the pinned SDK API. Physical
controller readiness, reported BD_ADDR and RF behavior remain PWT-005 acceptance
work.

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

PWT-002 also records one pinned-upstream limitation: the CYW43 shared-bus
implementation detects an internal TX queue-full condition, but its public
`cyw43_bluetooth_hci_write()` path does not propagate that internal result to
the caller. PWT-002 does not copy private CYW43 code to bypass this. PWT-004 must
therefore serialize bridge submission conservatively and own end-to-end
backpressure/retry policy; physical stress acceptance must revisit this boundary.

Prefer queues of complete HCI packets or length-delimited ring slots over arbitrary byte FIFOs that can lose framing after one corruption.

## PWT-004 integrated bridge contract

PWT-004 joins the PWT-002 controller transport and PWT-003 USB surface without
moving CYW43 framing into USB code or TinyUSB ownership into the CYW43 adapter.

### Bounded queues and ownership

The production bridge uses complete HCI packets in statically bounded storage:

| Queue/buffer | Capacity | Ownership purpose |
|---|---:|---|
| host command bridge queue | 2 packets | decouples EP0 command callbacks from CYW43 submission |
| host ACL bridge queue | 4 packets | controller-bound ACL buffering |
| controller→host bridge queue | 4 packets | drains CYW43 before USB IN availability |
| router pending command | 1 packet | protects the void TinyUSB command callback when the bridge queue is temporarily full |
| router ACL assembly | 1 packet | reassembles one HCI ACL packet across Full-Speed 64-byte USB OUT transfers |
| router USB egress | 1 packet | stable storage retained until TinyUSB IN completion |

The bridge and router share the PWT-002 typed HCI packet kind. CYW43's private
four-byte transport prefix still exists only inside `hci_transport.c`.

TinyUSB callback memory is synchronously copied before a callback returns.
Controller→host data is copied from the bridge queue into the router-owned USB
egress buffer before the bridge slot is released. A controller reset can
therefore clear/reuse bridge storage without invalidating memory owned by an
in-flight TinyUSB IN transfer.

### Host→controller USB backpressure

The pinned TinyUSB BTH driver arms ACL OUT using one
`CFG_TUD_BTH_DATA_EPSIZE` buffer (64 bytes for the MVP). A complete HCI ACL
packet is not therefore assumed to arrive in one callback. The router reads the
HCI ACL length field and reassembles fragments until exactly one complete packet
is present; overrun, impossible length, or malformed framing becomes a recovery
fault rather than being interpreted as another packet.

The upstream pinned BTH class automatically re-arms ACL OUT immediately after
each callback, which gives the application no way to stop the host when all
bounded storage is occupied. PWT-004 carries the revision-locked patch
`patches/tinyusb-bth-acl-backpressure.patch`. It removes only that automatic
re-arm and exposes `tud_bt_acl_data_receive_ready()`. picoWutooth re-arms the
endpoint only when its ACL assembly/bridge path can retain the next transfer.
When it cannot, leaving OUT unarmed causes normal USB NAK backpressure; no
received fragment is discarded.

HCI commands arrive over EP0 through a TinyUSB callback that cannot be
retroactively NAKed. The router therefore owns one complete pending-command
slot in addition to the two bridge command slots. HCI command-credit semantics
normally prevent this path from filling. A further command that cannot be
retained is treated as an observable transport fault and triggers recovery
rather than silent loss.

### Conservative CYW43 transmit pacing

The PWT-002 public CYW43 boundary cannot report the shared-bus internal
`CYBT_ERR_QUEUE_FULL` condition. PWT-004 therefore never relies on that
unobservable result.

Only one host→controller HCI packet is submitted to the public CYW43 write path
at a time:

- after a command, the bridge waits for a matching HCI Command Complete or
  Command Status event that returns a non-zero command credit;
- after an ACL packet, the bridge waits for Number Of Completed Packets for the
  matching connection handle.

Controller events are read before the next send decision, so the event that
returns a credit can release the pacing gate in the same service iteration.
This is intentionally conservative and preserves packet order within command
and ACL classes. Performance tuning can relax it only after PWT-005 physical
evidence establishes a safe controller-buffer contract.

### Reset and recovery

The CYW43439 transport must reach `READY` before TinyUSB is initialized, so a
host cannot enumerate a controller whose bootstrap is incomplete.

Normal host HCI Reset remains ordinary bridged HCI traffic. Transport recovery
is different: malformed framing, an ownership mismatch, a CYW43 transport
error, or an unrecoverable USB callback condition causes a visible USB
disconnect, clears bounded queues according to their ownership rules, performs
the PWT-002 full controller reset/rebootstrap, waits through a bounded disconnect
interval, and reconnects USB only after the controller is ready again.

Queued but not externally owned packets are discarded on reset and counted.
No queue or buffer is silently overwritten. Recovery counters and discarded
packet counts remain available internally without adding any production USB
debug interface.

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

PWT-002 establishes the firmware-side identity rule: use the CYW43 driver's
device-specific WLAN MAC and program Bluetooth to WLAN MAC + 1 using the same
octet-5 increment used by the pinned Pico SDK CYW43 BTstack transport. Never
hard-code a shared BD_ADDR.

PWT-005 must still read the controller's BD_ADDR on physical hardware and prove
that the exact candidate reports the intended non-shared identity. It must also
record HCI `Read Local Version Information`, supported commands/features and
buffer sizes so future regressions can distinguish firmware/controller changes
from bridge changes.

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
