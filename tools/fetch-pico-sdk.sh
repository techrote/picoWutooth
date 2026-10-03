#!/usr/bin/env bash
set -euo pipefail

SDK_TAG="2.3.1"
SDK_COMMIT="079c6f39023649b154152db30f1d781e884879bc"
TINYUSB_COMMIT="86ad6e56c1700e85f1c5678607a762cfe3aa2f47"
CYW43_COMMIT="055d64274b014dd7b1c2fc94d26e8a18face7124"
BTSTACK_COMMIT="eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_DIR="${PICO_SDK_PATH:-${ROOT_DIR}/.deps/pico-sdk}"
TINYUSB_NO_ISO_PATCH="${ROOT_DIR}/patches/tinyusb-bth-no-iso.patch"
TINYUSB_ACL_BACKPRESSURE_PATCH="${ROOT_DIR}/patches/tinyusb-bth-acl-backpressure.patch"

mkdir -p "$(dirname "${SDK_DIR}")"

if [[ -e "${SDK_DIR}" && ! -d "${SDK_DIR}/.git" ]]; then
    echo "error: ${SDK_DIR} exists but is not a git checkout" >&2
    exit 1
fi

if [[ ! -d "${SDK_DIR}/.git" ]]; then
    git clone --branch "${SDK_TAG}" --depth 1 https://github.com/raspberrypi/pico-sdk.git "${SDK_DIR}"
else
    git -C "${SDK_DIR}" fetch --depth 1 origin "refs/tags/${SDK_TAG}:refs/tags/${SDK_TAG}"
fi

git -C "${SDK_DIR}" checkout --detach "${SDK_COMMIT}"
git -C "${SDK_DIR}" submodule sync --recursive
git -C "${SDK_DIR}" submodule update --init --recursive --depth 1

verify_commit() {
    local path="$1"
    local expected="$2"
    local actual
    actual="$(git -C "${path}" rev-parse HEAD)"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "error: ${path} is ${actual}, expected ${expected}" >&2
        exit 1
    fi
}

apply_patch_idempotent() {
    local repository="$1"
    local patch="$2"
    local label="$3"

    if git -C "${repository}" apply --check "${patch}"; then
        git -C "${repository}" apply "${patch}"
    elif git -C "${repository}" apply --reverse --check "${patch}"; then
        echo "${label} already applied"
    else
        echo "error: ${repository} does not accept ${label} cleanly" >&2
        exit 1
    fi
}

verify_commit "${SDK_DIR}" "${SDK_COMMIT}"
verify_commit "${SDK_DIR}/lib/tinyusb" "${TINYUSB_COMMIT}"
verify_commit "${SDK_DIR}/lib/cyw43-driver" "${CYW43_COMMIT}"
verify_commit "${SDK_DIR}/lib/btstack" "${BTSTACK_COMMIT}"

apply_patch_idempotent     "${SDK_DIR}/lib/tinyusb"     "${TINYUSB_NO_ISO_PATCH}"     "TinyUSB no-ISO patch"

apply_patch_idempotent     "${SDK_DIR}/lib/tinyusb"     "${TINYUSB_ACL_BACKPRESSURE_PATCH}"     "TinyUSB ACL backpressure patch"

echo "Pico SDK ready: ${SDK_DIR}"
echo "  pico-sdk:     ${SDK_COMMIT}"
echo "  tinyusb:      ${TINYUSB_COMMIT} + picoWutooth no-ISO/backpressure patches"
echo "  cyw43-driver: ${CYW43_COMMIT}"
echo "  btstack:      ${BTSTACK_COMMIT}"
