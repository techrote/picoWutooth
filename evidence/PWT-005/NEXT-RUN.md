# Resume PWT-005 on compatible hardware

PWT-005 is not accepted. PR #15 remains draft; #6 remains open. No #7 work
was started. The attempted board was an ESP8266 clone, not a genuine Pico W.
The original exact CI candidate was flashed before this was discovered;
the clone now does not enumerate because production firmware requires CYW43
startup before USB. Holding BOOTSEL while reconnecting makes its RP2040
bootloader available for restoring appropriate clone firmware. No previous
clone firmware image was available to restore automatically.

## Inputs to establish

- A physically identified **Raspberry Pi Pico W or Pico WH with CYW43439**.
  Do not rely solely on the USB product string: the clone claimed "Pico W".
- A known, connectable BLE peripheral advertising nearby. The K22 keyboard/
  mouse is only a possible target; its BLE capability/address are unverified.
  A 2.4 GHz receiver alone does not establish BLE support.
- The final exact PR-head CI artifact identified in the latest #6/PR handoff.
  The new firmware fixes premature ACL endpoint arming; do not silently reuse
  the failed original artifact or infer compatibility from native tests.

## Host prepared in this attempt

Windows 11 Pro build 26200; WSL absent; firmware virtualization disabled.
usbipd-win 5.3.0 is installed. A QEMU 11.1.0 TCG guest runs Alpine 3.24.2,
kernel `6.18.52-0-lts`, BlueZ 5.86. `btusb`, `bluetooth`, `vhci_hcd` load;
D-Bus/bluetoothd and USBIP tools are installed. Actual Pico raw USB attachment
and controller operation **must still be proven**. This is a volatile RAM guest,
not a persistent VM installation; a PC/guest restart loses its setup.

Local download/setup files remain under `C:\picoWutooth\artifacts\host-tools\`.
The guest serial console is on Windows loopback port 4444. The local
`vm-console.py` helper records console output; no hardware acceptance is implied
by the console. Local file-transfer helpers are ephemeral and may need restart.
Use a normal native Linux host instead if available; the same repository capture
harness applies. Do not substitute Windows Bluetooth tests for this gate.

## Campaign

1. Reconcile live main, #6, PR #15/head and CI. Read the mandatory corpus.
2. Download the new exact-head artifact; inspect `build-manifest.txt`, confirm
   `source_commit`, and independently verify the UF2 SHA-256 before flashing.
3. Record the genuine board identity. Enter BOOTSEL by a supported software
   reset or the minimum physical BOOTSEL/replug action. Flash that verified UF2.
   Production firmware has no CDC interface; COM10 is not expected to remain.
4. For this Windows/VM path, inspect `usbipd list` and identify the **actual**
   `CAFE:4013` bus. Bind only that board from an administrator PowerShell:

   ```powershell
   usbipd bind --busid <observed-busid>
   usbipd list
   ```

   From Linux (the QEMU guest reaches Windows at `10.0.2.2`):

   ```sh
   modprobe vhci-hcd
   modprobe btusb
   usbip list -r 10.0.2.2
   usbip attach -r 10.0.2.2 -b <observed-busid>
   lsusb -d cafe:4013
   lsusb -t
   ```

   Prove standard `btusb` binding and the board-associated `hciN`; a loaded
   module without the attached board is not PASS.
5. Use an exact Git checkout in Linux, not a Windows CRLF source snapshot.
   Install bash, USB utilities, BlueZ (including hciconfig/hcitool), timeout and
   Git. Run as root where necessary so HCI and kernel evidence is accessible:

   ```sh
   bash tools/pwt005-linux-capture.sh --uf2 /path/to/picowutooth.uf2 --phase initial
   ```

   Inspect intended E0/01/01 USB topology, `btusb`, successful HCI Reset and
   information replies, buffers/features/commands and device-specific BD_ADDR.
   The harness includes an explicit kernel HCI reset and local-information
   reads inside btmon capture. Observe real advertisers.
6. Establish the controlled advertiser's physical identity, address and LE
   address type from the environment/scan. Run again with that actual address:

   ```sh
   bash tools/pwt005-linux-capture.sh --uf2 /path/to/picowutooth.uf2 \
     --phase initial --device <actual-address> --device-type le_public
   ```

   Use `le_random` when appropriate. `hcitool lecc` explicitly uses that peer
   type; `btmgmt` records connection info/list/disconnect. Verify successful
   LE Connection Complete and Disconnection Complete in btmon, not just exits.
7. Physically unplug/replug the genuine board without reflashing. Restore its
   USBIP attachment using the newly observed bus if required. Repeat with
   `--phase replug` and the controlled peripheral. Compare the controller address.
8. Preserve every failure before edits; classify with docs/02-VALIDATION.md.
   For a firmware fix, regression-test it, obtain the new exact green CI artifact,
   and repeat physical acceptance from the start. Do not weaken a gate.
9. Only after every physical criterion is evidenced and final exact-head CI is
   green: mark PR #15 ready, merge, verify main CI, update #1 and close #6.

The failed attempt's evidence is retained separately. No controller identity,
RF discovery, controlled connection/disconnect or accepted replug was observed.
