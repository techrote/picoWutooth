# Deterministic checks from this attempt

On Alpine 3.24.2, GCC 15.2.0, CMake 4.2.3, kernel 6.18.52-0-lts:

- The new native TinyUSB lifecycle test failed against the original adapter
  before configuration at its no-recovery assertion. The four existing tests
  passed. After the mount guard, all **5/5 native tests passed**.
- The capture regression failed against the original harness: expected missing
  HCI status 3, observed status 0 after the first `git status` capture. After
  the subshell fix it preserved UF2/commit info, command outputs and failures,
  and reached the missing-controller result as expected.
- Controlled LE dispatch tests passed for public and random peer types;
  a simulated hcitool failure remained status 17. No RF connection was made.
- Linux harness and LE helper syntax checks passed.

These validate software behavior only. Final hosted CI and the new exact
artifact identity are recorded in the latest #6/PR #15 handoff.

Committed device/kernel text is excerpted or whitespace-normalized for
readability. Full preflash device output and kernel log remain in local
`artifacts/host-tools/raw-evidence/`, with hashes in `raw-checksums.txt`.
