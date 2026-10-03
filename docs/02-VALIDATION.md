# Validation and evidence

## Principle

Separate tests by layer so a failure has a narrow diagnosis. Compilation is not hardware evidence, and host enumeration is not proof of RF functionality.

## Automated CI baseline

PWT-001 must establish required checks that run on every PR and on `main`.

Minimum CI:

1. clean Pico W firmware configure/build;
2. native unit tests for host-independent logic;
3. descriptor/static contract tests;
4. warnings treated deliberately (prefer `-Wall/-Wextra` or project-equivalent without importing noisy third-party warnings as failures);
5. artifact generation for the UF2 and useful build metadata.

Add formatting/lint/static analysis only where it is stable and high-signal. Do not create a large style gate that obstructs transport work.

Pin/toolchain versions sufficiently to make CI failures attributable.

## Native deterministic tests

Hardware-independent tests should cover at least:

### CYW43 framing

- adds exactly one four-byte CYW43 pre-header;
- packet type byte is correct for command/ACL/event paths;
- removes the pre-header exactly once;
- rejects short/oversized/malformed frames;
- preserves payload byte-for-byte.

### USB HCI routing

- HCI command path maps to controller command packet type;
- ACL OUT maps to controller ACL packet type;
- controller event maps only to interrupt IN;
- controller ACL maps only to bulk IN;
- length boundaries are checked.

### Queue/backpressure state machine

- full queue never overwrites an owned packet;
- destination-busy state retries or naturally exerts backpressure;
- completion releases exactly one buffer;
- reset clears or drains state according to documented policy;
- wraparound preserves packet boundaries/order;
- repeated busy/ready transitions do not deadlock;
- fault counters increment deterministically.

### Descriptor contract

Parse the built descriptor representation or a generated fixture and assert:

- Bluetooth HCI class/subclass/protocol;
- expected interface count;
- endpoint direction/type;
- no accidental CDC/vendor interface in production build;
- ISO alternatives absent for MVP unless explicitly activated later.

## Build matrix

At minimum:

| Build | Purpose |
|---|---|
| production | single-purpose Bluetooth HCI USB identity |
| native tests | framing/queue/state machine tests on CI host |
| optional diagnostic | UART/SWD diagnostics; must not be release artifact unless clearly named |

Avoid a combinatorial configuration matrix.

## Physical evidence format

Hardware acceptance evidence must identify:

- repository commit SHA;
- generated UF2 checksum;
- Pico W/Pico WH board identity if relevant;
- Pico SDK/TinyUSB dependency revisions;
- host OS and version/build;
- exact commands/procedure;
- captured output/logs/screenshots as appropriate;
- pass/fail result and observed anomalies.

Store small textual evidence in `evidence/<issue-id>/` or a clearly named equivalent. Large binary captures should be summarized and checksummed rather than casually committed.

No issue may convert "expected", "should", or simulator behaviour into a hardware PASS.

## PWT-005 Linux acceptance

Required sequence on real Pico W hardware:

1. flash exact candidate UF2;
2. verify USB descriptors with standard host tooling;
3. verify Linux binds the standard Bluetooth USB driver rather than a project-specific driver;
4. capture kernel/USB/controller enumeration;
5. obtain controller information after HCI initialization;
6. verify HCI Reset/command-complete path;
7. start BLE scan and observe at least one known/controlled advertiser where practical;
8. connect to at least one controlled BLE peripheral or test target;
9. capture `btmon` (or equivalent) evidence around reset, scan and connection;
10. unplug/replug and repeat a minimum smoke path.

If Linux unexpectedly requires a quirk/custom driver, stop and classify the protocol/descriptor incompatibility before proceeding to Windows acceptance.

## PWT-006 Windows 11 acceptance

Required sequence on real Pico W hardware:

1. flash the exact Linux-accepted candidate or a documented descendant containing only relevant fixes;
2. verify Device Manager identifies the adapter under Bluetooth and uses the in-box Microsoft Bluetooth USB driver;
3. verify no custom INF/project driver is required;
4. record Windows edition/build and relevant device/driver identity;
5. perform BLE discovery;
6. pair/connect to a controlled BLE device;
7. verify disable/enable or equivalent controller restart;
8. verify unplug/replug recovery;
9. reboot with the adapter attached and verify recovery;
10. inspect Event Viewer/device errors if any failure occurs.

A screenshot alone is insufficient. Pair it with textual device/driver identifiers and exact firmware SHA.

## PWT-007 breadth and robustness

Representative scenarios:

- repeated scan start/stop;
- repeated connect/disconnect;
- multiple discovered devices;
- sustained ACL traffic appropriate to an ordinary BLE/GATT workload;
- HCI Reset while idle and after traffic;
- USB reset/re-enumeration;
- controller error/recovery path where safely inducible;
- BR/EDR inquiry/connection if controller capabilities and host policy permit;
- representative Classic HID or similarly simple BR/EDR profile through the host stack.

Stress tests should assert recovery and packet-accounting invariants rather than chase maximum throughput.

## Release acceptance

PWT-008 must confirm:

- all required CI green on release commit;
- Linux and Windows evidence refers to the release commit or a bit-identical/reproducibly identical artifact;
- UF2 checksum recorded;
- dependency revisions/licenses recorded;
- build instructions reproduce the artifact;
- limitations are explicit, especially audio, Wi-Fi coexistence and tested host versions.

## Failure classification

When a hardware test fails, classify before editing:

- USB enumeration/descriptor;
- host driver binding;
- HCI command path;
- HCI event path;
- ACL TX/RX;
- CYW43 initialization;
- queue/backpressure;
- RF/environment/peripheral;
- host policy/profile;
- unknown.

Capture evidence before changing code whenever possible.
