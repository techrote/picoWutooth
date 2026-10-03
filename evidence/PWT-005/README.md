# PWT-005 Linux physical acceptance handoff

PWT-005 is hardware-gated. This directory intentionally contains no PASS claim
until evidence from a real Raspberry Pi Pico W and Linux host is committed.

## Accepted software baseline entering PWT-005

PWT-004 merged on `main` as:

- source commit: `8e5fe671a2adfecb859eb0ad433bb9007a849cca`
- post-merge CI run: `37141529609`
- CI artifact: `picowutooth-firmware-8e5fe671a2adfecb859eb0ad433bb9007a849cca`
- artifact ID: `11279669904`
- artifact archive SHA-256: `4548148b3329c92cd7a898356ac72ae89363c6b48c0d038f97d0cc9c960afefa`
- UF2 SHA-256 from the build manifest:
  `3db3f50cb0925e4f6bb80e8bafde8d75ccbacb3d4fd513e763ceeb6902058edd`

PWT-005 host tooling changes create a new PR-head build identity. **Flash the
final PWT-005 PR-head UF2 identified in the issue handoff comment**, not an
arbitrary locally rebuilt image, so hardware evidence maps to the exact
candidate that CI validated.

## Host requirements

A Linux machine with the Pico W connected directly enough that USB replug can be
performed, plus standard tools:

- `usbutils` / `lsusb`;
- BlueZ `btmgmt`, `btmon`, and `bluetoothctl`;
- `journalctl` where available;
- `sha256sum`.

Some distributions require root or capabilities for `btmgmt`, `btmon`, or
kernel logs. The capture script retains failures rather than hiding them.

## Acceptance sequence

1. Download the exact PR-head firmware artifact named in the #6 handoff and
   extract `picowutooth.uf2` plus `build-manifest.txt`.
2. Verify the UF2 SHA-256 equals the manifest value.
3. Flash that UF2 to a real Pico W.
4. Run:

   ```sh
   tools/pwt005-linux-capture.sh \
     --uf2 /path/to/picowutooth.uf2 \
     --device XX:XX:XX:XX:XX:XX \
     --device-type le_public \
     --phase initial
   ```

   Use `le_random` instead when the controlled peripheral advertises with a
   random address.
5. Inspect the generated run directory before changing firmware. Classify any
   failure using `docs/02-VALIDATION.md`.
6. Physically unplug/replug the Pico W without reflashing it.
7. Repeat the same command with `--phase replug`.
8. Commit the concise textual evidence needed to establish the issue gates.
   Large binary traces should be checksummed/summarized rather than committed
   casually.

The capture harness identifies picoWutooth by the current engineering USB
identity `CAFE:4013`, maps it to its `hciN` sysfs node, records the bound
driver, power-cycles the controller while `btmon` runs, records management
information, performs LE discovery, and optionally connects/disconnects a
controlled LE device.

## PASS evidence required before merge

Do not merge this PWT-005 branch merely because its CI is green. Evidence must
show, on an exact candidate:

- USB enumeration with the intended descriptor topology;
- standard Linux `btusb` binding;
- successful HCI reset/controller initialization;
- controller version/features/buffer/BD_ADDR information visible in the
  management/`btmon` capture;
- a device-specific controller address;
- successful LE discovery containing a known/controlled advertiser;
- successful controlled BLE connection;
- successful disconnect;
- successful physical unplug/replug followed by the smoke path again;
- exact source commit, UF2 SHA-256, Pico SDK/TinyUSB revisions, board and Linux
  host version.

Until those records exist, PWT-005 remains **not accepted**.
