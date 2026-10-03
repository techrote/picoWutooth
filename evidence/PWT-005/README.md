# PWT-005 Linux physical acceptance handoff

PWT-005 is hardware-gated. This directory contains curated evidence and handoff
material only; it intentionally contains no physical PASS claim until a genuine
Raspberry Pi Pico W/WH with CYW43439 completes the campaign.

## Accepted software baseline entering PWT-005

PWT-004 merged on `main` as:

- source commit: `8e5fe671a2adfecb859eb0ad433bb9007a849cca`
- post-merge CI run: `37141529609`
- CI artifact: `picowutooth-firmware-8e5fe671a2adfecb859eb0ad433bb9007a849cca`
- artifact ID: `11279669904`
- artifact archive SHA-256: `4548148b3329c92cd7a898356ac72ae89363c6b48c0d038f97d0cc9c960afefa`
- UF2 SHA-256: `3db3f50cb0925e4f6bb80e8bafde8d75ccbacb3d4fd513e763ceeb6902058edd`

PR #15 contains subsequent PWT-005 fixes/tooling. Always use the **latest exact
PR-head CI artifact named in #6/PR #15**, not an older artifact or an arbitrary
local rebuild.

## Host requirements

Use Linux with the genuine Pico W attached directly or through transparent USB
passthrough. Required tools:

- `usbutils` / `lsusb`;
- BlueZ `btmgmt`, `btmon`, and `bluetoothctl`;
- BlueZ `hciconfig` and `hcitool` for explicit HCI reset/info and controlled
  LE link establishment;
- `journalctl` or `dmesg`;
- `sha256sum`.

Some distributions require root or capabilities for Bluetooth management and
kernel logs.

## Raw captures vs committed evidence

`tools/pwt005-linux-capture.sh` writes raw runs beneath
`artifacts/PWT-005/runs/` by default. That tree is gitignored. This prevents
machine-specific USB inventory, local paths, peripheral identifiers and large
diagnostic output from entering the repository accidentally.

Inspect the raw run first. Commit only the concise, relevant textual evidence
needed to establish the PWT-005 gates, with sensitive/unrelated host identifiers
omitted. An explicit `--output` may be used when a different raw-capture
location is required.

## Acceptance sequence

1. Download the exact latest PR-head artifact identified in #6/PR #15 and
   extract `picowutooth.uf2` plus `build-manifest.txt`.
2. Verify source commit and UF2 SHA-256 against the manifest.
3. Flash that UF2 to a physically identified genuine Pico W/WH with CYW43439.
4. Run:

   ```sh
   bash tools/pwt005-linux-capture.sh \
     --uf2 /path/to/picowutooth.uf2 \
     --device XX:XX:XX:XX:XX:XX \
     --device-type le_public \
     --phase initial
   ```

   Use `le_random` when the controlled peripheral advertises with a random
   address.
5. Inspect the raw capture and classify any failure using
   `docs/02-VALIDATION.md` before changing firmware.
6. Physically unplug/replug the Pico W without reflashing it.
7. Repeat the same command with `--phase replug`.
8. Curate the minimal evidence required below into `evidence/PWT-005/`.

The harness identifies picoWutooth by engineering USB identity `CAFE:4013`,
maps it to the corresponding `hciN`, records `btusb` binding and controller
information, captures HCI traffic with `btmon`, performs LE discovery, and can
connect/disconnect a controlled LE device.

## PASS evidence required before merge

Do not merge PR #15 merely because CI is green. Evidence must show, on one exact
candidate:

- intended USB enumeration/descriptor topology;
- standard Linux `btusb` binding;
- successful HCI reset/controller initialization;
- local version, supported features/commands, buffer information and BD_ADDR;
- a device-specific controller address;
- discovery of a known/controlled BLE advertiser;
- successful controlled BLE connection and disconnect;
- physical unplug/replug followed by the smoke path again;
- exact source commit, UF2 SHA-256, dependency revisions, board identity and
  Linux host/kernel/BlueZ versions.

Until those records exist, PWT-005 remains **not accepted**.

## Incompatible-board attempt

The first physical attempt used a clone that described itself over USB as
"Pico W" but was later identified as an RP2040 board paired with an **ESP8266,
not CYW43/CYW43439**. It cannot satisfy picoWutooth's Bluetooth transport
contract.

See `host-preflight/FAILURE.md` for the concise classification and
`host-preflight/software-validation.md` for the independent software defects
found during that attempt. Raw host inventories were deliberately not retained
in the repository.

A genuine Pico W/WH must repeat the complete campaign before PR #15 can leave
draft or PWT-005 can close.
