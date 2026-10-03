# Initial candidate failure, preserved before fixes

Candidate: `46200a0910c376831d33e90ff05b6cf4078ce538`; CI run
`37142057364`, artifact `11280563347`. UF2 SHA-256:
`3db3f50cb0925e4f6bb80e8bafde8d75ccbacb3d4fd513e763ceeb6902058edd`.

The physically attached board reported serial `504450611827BE1C`, parent
`USB\VID_239A&PID_CAFE\504450611827BE1C`, with COM10 and HID interfaces.
After a 1200-baud touch it appeared on bus 3-5 as `2e8a:0003` and the
`RPI-RP2` BOOTSEL drive. USB firmware described it as "Pico W"; this is not
proof of its physical radio hardware. The verified candidate was copied to that drive.
Neither `CAFE:4013` nor another Pico USB device appeared afterwards. The user
physically unplugged it for five seconds and replugged without BOOTSEL;
the board remained absent from the USBIP device list. These Windows identity
observations are preflight/failure-localization evidence, not Linux acceptance.

Linux environment: Alpine 3.24.2 in QEMU 11.1.0 TCG on this Windows 11 Pro
26200 PC. Hardware virtualization reports disabled, WSL is absent. Kernel
`6.18.52-0-lts`; BlueZ 5.86; standard `btusb`, `bluetooth`, `vhci_hcd`
modules load successfully. D-Bus and bluetoothd run. usbipd-win 5.3.0 responds
to Linux `usbip list -r 10.0.2.2`. Actual candidate USB attachment was impossible
because the candidate did not enumerate. Raw USB attachment remains unproven.

The user subsequently identified this board as a clone fitted with an
**ESP8266, not CYW43**. PWT-005 requires a genuine Pico W/WH with CYW43439.
The reported USB name/serial did not establish radio identity. Further
flashing of this incompatible board was stopped.

Classification under docs/02-VALIDATION.md: **CYW43 initialization — incompatible
hardware/environment**. Non-enumeration is consistent with the candidate's
controller-before-USB startup contract; it is not evidence of a CYW43 firmware
failure on compatible hardware. No actual startup return code was observed.
Code inspection independently
identified a deterministic enumeration blocker: the production
service loop tries to arm ACL OUT before USB configuration, when the patched
TinyUSB BTH endpoint is zero. Its false result is classified as a transport
fault, causing USB disconnect/controller recovery before enumeration completes.
A native test against the actual TinyUSB adapter reproduced this before the fix;
see `software-validation.md`.
This separate software defect does not establish the cause of this board's
failure and cannot make an ESP8266 board compatible with this project.

The exact-checkout harness run in `runs/20261003T182349Z-initial` also exposed
a capture-tool defect: `capture()` uses `exit` in a brace group and exits the
entire harness after the first captured command (`git status`, status 0).
Its partial summary is not a completed campaign. Preserve failures and return
from each capture in a subshell; add an execution regression test.

An earlier archive-only trial failed on CRLF shell lines and used a synthetic
Git snapshot. It is not acceptance evidence. Linux was then cloned from a Git
bundle containing the genuine candidate commit, with normal Linux LF checkout.

The controlled-connect command was also inspected against installed BlueZ
5.86 and its primary `tools/hcitool.c` source. `btmgmt` provides disconnect,
connection listing and info but no `connect` command. The harness now uses
standard `hcitool lecc`, with `--random` only for a random peer address;
automated tests assert command dispatch and preservation of a failing status.
This is host-tool validation, not a real BLE connection.

All controller, BD_ADDR, BLE discovery/connection/disconnect and accepted
replug gates remain **NOT ESTABLISHED**. No firmware fix has yet been physically
verified. The user confirmed that only the incompatible clone is available.
The user supplied a K22 wireless keyboard/mouse as a possible target;
its BLE capability and identity have not been established by scanning.
