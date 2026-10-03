#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 2 ]]; then
    echo "usage: $0 <firmware.uf2> <manifest.txt>" >&2
    exit 2
fi

UF2="$1"
OUT="$2"
ROOT_DIR="$(git rev-parse --show-toplevel)"
SDK_DIR="${PICO_SDK_PATH:-${ROOT_DIR}/.deps/pico-sdk}"

if [[ ! -f "${UF2}" ]]; then
    echo "error: UF2 not found: ${UF2}" >&2
    exit 1
fi

mkdir -p "$(dirname "${OUT}")"

SOURCE_COMMIT="$(git -C "${ROOT_DIR}" rev-parse HEAD)"
SDK_COMMIT="$(git -C "${SDK_DIR}" rev-parse HEAD)"
TINYUSB_COMMIT="$(git -C "${SDK_DIR}/lib/tinyusb" rev-parse HEAD)"
CYW43_COMMIT="$(git -C "${SDK_DIR}/lib/cyw43-driver" rev-parse HEAD)"
BTSTACK_COMMIT="$(git -C "${SDK_DIR}/lib/btstack" rev-parse HEAD)"
UF2_SHA256="$(sha256sum "${UF2}" | awk '{print $1}')"
ARM_GCC="$(arm-none-eabi-gcc --version | head -n 1)"
CMAKE_VERSION="$(cmake --version | head -n 1)"

cat > "${OUT}" <<EOF
project=picoWutooth
source_commit=${SOURCE_COMMIT}
pico_board=pico_w
pico_sdk_commit=${SDK_COMMIT}
tinyusb_commit=${TINYUSB_COMMIT}
cyw43_driver_commit=${CYW43_COMMIT}
btstack_commit=${BTSTACK_COMMIT}
arm_gcc=${ARM_GCC}
cmake=${CMAKE_VERSION}
uf2_file=$(basename "${UF2}")
uf2_sha256=${UF2_SHA256}
EOF

cat "${OUT}"
