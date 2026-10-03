# picoWutooth

Use a Raspberry Pi Pico W as a standards-conforming USB Bluetooth HCI controller/dongle.

The intended data path is:

```text
host Bluetooth stack
    ↕ USB Bluetooth HCI class
RP2040 / TinyUSB
    ↕ HCI bridge
CYW43439
    ↕ RF
Bluetooth devices
```

## Programme status

The repository now contains the reproducible foundation, the PWT-002 CYW43-facing raw-HCI transport, the PWT-003 TinyUSB Bluetooth-HCI USB surface, and the PWT-004 bounded production bridge joining them. The integrated firmware is a **software/CI candidate only** until PWT-005 performs physical Linux/controller/RF acceptance; no physical Bluetooth functionality is inferred from the cross-build or native tests.

The MVP targets a single-purpose USB Bluetooth HCI device, with Linux used first for protocol-level diagnostics and Windows 11 as the primary generic-dongle acceptance target. BLE is the first functional milestone; BR/EDR follows. Wi-Fi coexistence, USB composite debug interfaces, and SCO/ISO audio are deliberately deferred until the basic controller path is stable.

Start with [RAG.md](RAG.md). It is the retrieval/index document for the programme and defines the authoritative read order for implementation issues.

## Foundation build

The dependency baseline is pinned in [docs/04-REFERENCES.md](docs/04-REFERENCES.md). The helper scripts fetch the exact Pico SDK commit and recursively verify the TinyUSB, CYW43-driver, and BTstack submodule revisions expected by that SDK.

### Linux / macOS shell

Prerequisites: Git, CMake, Ninja, and an Arm `arm-none-eabi` GCC toolchain supported by the pinned Pico SDK.

```sh
bash tools/fetch-pico-sdk.sh

cmake -S . -B build-firmware -G Ninja \
  -DPICO_SDK_PATH="$PWD/.deps/pico-sdk" \
  -DPICO_BOARD=pico_w \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-firmware --target picowutooth
```

The UF2 is emitted as `build-firmware/picowutooth.uf2`.

### Windows PowerShell

Prerequisites: Git, CMake, Ninja, and an Arm `arm-none-eabi` GCC toolchain on `PATH`.

```powershell
pwsh -File .\tools\fetch-pico-sdk.ps1

cmake -S . -B build-firmware -G Ninja `
  -DPICO_SDK_PATH="$PWD\.deps\pico-sdk" `
  -DPICO_BOARD=pico_w `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-firmware --target picowutooth
```

### Native tests

The host-native test configuration deliberately does not load the Pico SDK:

```sh
cmake -S . -B build-native -G Ninja \
  -DPWT_BUILD_NATIVE_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-native
ctest --test-dir build-native --output-on-failure
```

The native harness covers CYW43 framing/lifecycle, USB descriptor/routing contracts, and PWT-004 bridge buffering, ACL reassembly, backpressure, ownership, pacing, reset, and recovery state.

## CI artifacts and traceability

Every PR and push to `main` runs:

- **Native tests**
- **Pico W firmware**

The firmware job uploads the UF2 plus `build-manifest.txt`. The manifest records the repository commit, board target, exact Pico SDK/TinyUSB/CYW43-driver/BTstack revisions, compiler/CMake identity, and UF2 SHA-256.

For CI builds, `PWT_SOURCE_COMMIT` is also compiled into the image as a non-interface build-identity string. This does not add USB CDC, HID, or vendor interfaces.
