# Incompatible-board physical attempt

## Candidate

The first PWT-005 candidate physically attempted was:

- source: `46200a0910c376831d33e90ff05b6cf4078ce538`
- CI run: `37142057364`
- artifact ID: `11280563347`
- UF2 SHA-256: `3db3f50cb0925e4f6bb80e8bafde8d75ccbacb3d4fd513e763ceeb6902058edd`

The attached RP2040 board appeared on Windows as COM10 and identified itself
over USB as "Pico W". It successfully entered the standard RP2040 BOOTSEL
bootloader after a 1200-baud touch, and the verified candidate UF2 was copied to
the BOOTSEL drive.

After flashing, no picoWutooth USB device appeared. A real cold unplug/replug
without BOOTSEL produced the same result.

The board was then physically identified as a clone using an **ESP8266 rather
than CYW43/CYW43439**. The ESP8266 provides Wi-Fi but no Bluetooth controller,
so this board cannot satisfy picoWutooth's CYW43 transport contract.

## Classification

Under `docs/02-VALIDATION.md`, classify this attempt as:

**CYW43 initialization / incompatible hardware**.

The observed non-enumeration is not evidence of a firmware defect on compatible
Pico W hardware. The production design intentionally requires CYW43 controller
startup before exposing the Bluetooth USB device, and the required controller
does not exist on this clone.

No controller identity, BD_ADDR, Bluetooth RF discovery, controlled
connection/disconnection, or accepted replug result was obtained.

Do not use this clone for further PWT-005 acceptance. Restore clone-appropriate
firmware through RP2040 BOOTSEL if desired.

## Independent software findings from the attempt

Code/host-tool review during this failed campaign found three reproducible
software/tooling issues independent of the clone mismatch:

1. The TinyUSB adapter attempted ACL OUT re-arming before USB configuration.
   Before `SET_CONFIGURATION`, the BTH endpoint does not exist; treating that
   normal state as an arm failure can trigger premature recovery.
2. The original Linux capture helper used `exit` inside a brace-group capture
   function, causing the entire harness to terminate after its first capture.
3. The original controlled-connect path named a nonexistent `btmgmt connect`
   command. BlueZ's HCI LE connection utility is `hcitool lecc`, with explicit
   public/random peer address handling.

PR #15 fixes these issues and carries deterministic regressions. They must still
be physically validated on a genuine Pico W/WH with CYW43439 before PWT-005 can
be accepted.
