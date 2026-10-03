# Resume PWT-005 on compatible hardware

PWT-005 is not accepted. PR #15 remains draft, #6 remains open, and #7 has not
started.

The previous physical board was an ESP8266-equipped clone rather than a genuine
Pico W. It cannot exercise the CYW43/CYW43439 Bluetooth path.

## Required inputs

- A physically identified **Raspberry Pi Pico W or Pico WH with CYW43439**.
  Do not trust the USB product string alone.
- A known, connectable BLE peripheral whose address and public/random LE address
  type can be established.
- The **latest exact PR #15 CI artifact** named in the current #6/PR handoff.

The current branch includes a real pre-enumeration TinyUSB lifecycle fix and
host-tooling regressions discovered during the failed clone attempt. Do not
silently reuse an older candidate.

## Host

Prefer a native Linux host. A Linux VM/guest with transparent raw USB
passthrough is acceptable if it can prove the actual `CAFE:4013` device is
attached to Linux, bound by standard `btusb`, and represented by the expected
`hciN`.

If starting from Windows, `usbipd-win` plus a Linux guest is one possible
route. Re-establish that environment from scratch as needed; do not depend on
the earlier volatile VM state or machine-local helper paths.

Windows Bluetooth behavior is not a substitute for this Linux acceptance gate.

## Campaign

1. Reconcile live `main`, #6, PR #15/head and CI. Read the mandatory corpus.
2. Download the latest exact-head artifact. Verify `source_commit` and the UF2
   SHA-256 from `build-manifest.txt`.
3. Record the genuine board identity, enter BOOTSEL through a supported path,
   and flash that exact UF2. Production firmware has no CDC interface, so a
   pre-flash COM port is not expected to remain.
4. Attach the resulting `CAFE:4013` USB device to Linux and prove:
   - intended descriptors;
   - standard `btusb` binding;
   - board-associated `hciN`.
5. From an exact Linux checkout run:

   ```sh
   bash tools/pwt005-linux-capture.sh \
     --uf2 /path/to/picowutooth.uf2 \
     --phase initial
   ```

   Inspect HCI Reset and information replies, buffers/features/commands,
   device-specific BD_ADDR, kernel/BlueZ state and real advertisers.
6. Establish a controlled advertiser's physical identity, address and LE address
   type. Repeat with:

   ```sh
   bash tools/pwt005-linux-capture.sh \
     --uf2 /path/to/picowutooth.uf2 \
     --phase initial \
     --device <actual-address> \
     --device-type le_public
   ```

   Use `le_random` when appropriate. Verify successful LE Connection Complete
   and Disconnection Complete in `btmon`; do not infer success from tool exit
   status alone.
7. Physically unplug/replug the genuine board without reflashing. Restore USB
   passthrough if necessary and repeat with `--phase replug`.
8. Preserve each failure before editing and classify it with
   `docs/02-VALIDATION.md`. For a firmware fix, add a deterministic regression
   where practical, obtain a new exact green CI artifact, and repeat physical
   acceptance from the start.
9. Curate only the relevant evidence from the gitignored raw capture directory
   into `evidence/PWT-005/`.
10. Only after every physical criterion and final exact-head CI pass: mark PR
    #15 ready, merge, verify post-merge `main` CI, update #1 and close #6.

The incompatible-board attempt established no controller identity, RF discovery,
controlled BLE connection/disconnect or accepted replug result.
