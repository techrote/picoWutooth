# Deterministic software validation from the incompatible-board attempt

The hardware attempt could not establish Bluetooth acceptance because the board
was an ESP8266-equipped clone. It nevertheless exposed reproducible software and
host-tool defects that were validated independently of hardware.

On Alpine Linux 3.24.2 with GCC 15.2.0, CMake 4.2.3 and kernel
6.18.52-0-lts:

- The new TinyUSB lifecycle regression failed against the original adapter at
  the pre-configuration no-recovery assertion. After gating ACL OUT re-arming
  on USB configuration, all **5/5 native tests passed**.
- The capture regression reproduced the original harness terminating after its
  first captured command. After moving each capture into a subshell, the
  harness retained successes/failures and continued to the intended terminal
  condition.
- Controlled LE dispatch tests passed for public and random peer address types.
  A simulated `hcitool` failure remained observable as the original non-zero
  status; no RF connection was inferred.
- Linux capture/LE helper syntax and shell regressions passed.
- Hosted exact-head CI subsequently passed both required jobs; the latest #6
  issue comment and PR #15 body identify the current immutable candidate.

These checks establish software behavior only. They do **not** satisfy any
PWT-005 physical Bluetooth gate.
