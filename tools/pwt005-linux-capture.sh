#!/usr/bin/env bash
set -uo pipefail

VID="cafe"
PID="4013"
OUTPUT_ROOT=""
UF2=""
HCI=""
DEVICE=""
DEVICE_TYPE="le_public"
PHASE="initial"
SCAN_SECONDS=15

usage() {
    cat <<'EOF'
Usage:
  tools/pwt005-linux-capture.sh [options]

Options:
  --output DIR              Evidence output directory.
  --uf2 PATH                UF2 file that was flashed; records SHA-256.
  --hci hciN                Force the target HCI device instead of sysfs detection.
  --device XX:XX:XX:XX:XX:XX
                            Controlled BLE peripheral to connect/disconnect.
  --device-type TYPE        public|random|le_public|le_random (default: le_public).
  --phase NAME              Evidence phase, e.g. initial or replug (default: initial).
  --scan-seconds N          LE discovery duration (default: 15).
  -h, --help                Show this help.

Run as a normal user first. Some distributions require sudo/CAP_NET_ADMIN for
btmgmt/btmon and restrict kernel logs; failures are retained in the evidence.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) OUTPUT_ROOT="$2"; shift 2 ;;
        --uf2) UF2="$2"; shift 2 ;;
        --hci) HCI="$2"; shift 2 ;;
        --device) DEVICE="$2"; shift 2 ;;
        --device-type) DEVICE_TYPE="$2"; shift 2 ;;
        --phase) PHASE="$2"; shift 2 ;;
        --scan-seconds) SCAN_SECONDS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "error: PWT-005 capture requires Linux" >&2
    exit 2
fi

if [[ ! "$SCAN_SECONDS" =~ ^[0-9]+$ ]] || [[ "$SCAN_SECONDS" -lt 1 ]]; then
    echo "error: --scan-seconds must be a positive integer" >&2
    exit 2
fi

case "$DEVICE_TYPE" in
    public|random|le_public|le_random) ;;
    *) echo "error: invalid --device-type: $DEVICE_TYPE" >&2; exit 2 ;;
esac

timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
if [[ -z "$OUTPUT_ROOT" ]]; then
    OUTPUT_ROOT="evidence/PWT-005/runs/${timestamp}-${PHASE}"
fi
mkdir -p "$OUTPUT_ROOT"

SUMMARY="$OUTPUT_ROOT/summary.txt"
: > "$SUMMARY"

record() {
    printf '%s=%s\n' "$1" "$2" | tee -a "$SUMMARY"
}

capture() {
    local name="$1"
    shift
    {
        printf '$'
        printf ' %q' "$@"
        printf '\n'
        "$@"
        status=$?
        printf '\nexit_status=%d\n' "$status"
        exit "$status"
    } >"$OUTPUT_ROOT/$name" 2>&1
}

capture_optional() {
    local name="$1"
    shift
    capture "$name" "$@" || true
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

record phase "$PHASE"
record utc_timestamp "$timestamp"
record hostname "$(hostname 2>/dev/null || printf unknown)"
record kernel "$(uname -srmo)"

if [[ -r /etc/os-release ]]; then
    cp /etc/os-release "$OUTPUT_ROOT/os-release.txt"
fi

if command_exists git && git rev-parse --show-toplevel >/dev/null 2>&1; then
    record repository_commit "$(git rev-parse HEAD)"
    capture_optional "git-status.txt" git status --short --branch
else
    record repository_commit "unavailable"
fi

if [[ -n "$UF2" ]]; then
    if [[ ! -f "$UF2" ]]; then
        echo "error: UF2 not found: $UF2" >&2
        exit 2
    fi
    uf2_sha="$(sha256sum "$UF2" | awk '{print $1}')"
    record uf2_file "$(basename "$UF2")"
    record uf2_sha256 "$uf2_sha"
else
    record uf2_file "not-supplied"
    record uf2_sha256 "not-supplied"
fi

for cmd in lsusb btmgmt btmon bluetoothctl timeout; do
    if command_exists "$cmd"; then
        record "tool_$cmd" "$(command -v "$cmd")"
        capture_optional "version-$cmd.txt" "$cmd" --version
    else
        record "tool_$cmd" "MISSING"
    fi
done

if command_exists lsusb; then
    capture_optional "usb-device.txt" lsusb -d "${VID}:${PID}"
    capture_optional "usb-descriptors.txt" lsusb -v -d "${VID}:${PID}"
    capture_optional "usb-tree.txt" lsusb -t
fi

capture_optional "bluetoothctl-list.txt" bluetoothctl list

find_usb_ancestor() {
    local path
    path="$(readlink -f "/sys/class/bluetooth/$1/device" 2>/dev/null)" || return 1
    while [[ "$path" != "/" && -n "$path" ]]; do
        if [[ -r "$path/idVendor" && -r "$path/idProduct" ]]; then
            local vendor product
            vendor="$(tr '[:upper:]' '[:lower:]' < "$path/idVendor")"
            product="$(tr '[:upper:]' '[:lower:]' < "$path/idProduct")"
            if [[ "$vendor" == "$VID" && "$product" == "$PID" ]]; then
                printf '%s\n' "$path"
                return 0
            fi
        fi
        path="$(dirname "$path")"
    done
    return 1
}

detect_hci() {
    local candidate usb_path found=""
    shopt -s nullglob
    for node in /sys/class/bluetooth/hci*; do
        candidate="$(basename "$node")"
        usb_path="$(find_usb_ancestor "$candidate" || true)"
        if [[ -n "$usb_path" ]]; then
            if [[ -n "$found" ]]; then
                echo "error: multiple HCI devices match ${VID}:${PID}; use --hci" >&2
                return 2
            fi
            found="$candidate"
        fi
    done
    shopt -u nullglob
    [[ -n "$found" ]] || return 1
    printf '%s\n' "$found"
}

if [[ -z "$HCI" ]]; then
    HCI="$(detect_hci || true)"
fi

if [[ -z "$HCI" || ! -d "/sys/class/bluetooth/$HCI" ]]; then
    record hci_device "NOT_FOUND"
    record driver_binding "NOT_FOUND"
    capture_optional "kernel-log.txt" journalctl -k --since "-10 min" --no-pager
    echo "No Bluetooth HCI device for USB ${VID}:${PID} was found." >&2
    echo "Evidence retained in $OUTPUT_ROOT" >&2
    exit 3
fi

record hci_device "$HCI"
INDEX="${HCI#hci}"
record hci_index "$INDEX"

USB_PATH="$(find_usb_ancestor "$HCI" || true)"
record usb_sysfs_path "${USB_PATH:-NOT_FOUND}"

driver=""
walk="$(readlink -f "/sys/class/bluetooth/$HCI/device" 2>/dev/null || true)"
while [[ -n "$walk" && "$walk" != "/" ]]; do
    if [[ -L "$walk/driver" ]]; then
        driver="$(basename "$(readlink -f "$walk/driver")")"
        break
    fi
    walk="$(dirname "$walk")"
done
record driver_binding "${driver:-UNKNOWN}"

if [[ -r "/sys/class/bluetooth/$HCI/address" ]]; then
    CONTROLLER_ADDRESS="$(cat "/sys/class/bluetooth/$HCI/address")"
    record controller_address "$CONTROLLER_ADDRESS"
else
    CONTROLLER_ADDRESS=""
    record controller_address "UNKNOWN"
fi

capture_optional "sysfs-device.txt" sh -c "readlink -f '/sys/class/bluetooth/$HCI/device'; find '/sys/class/bluetooth/$HCI' -maxdepth 1 -type f -print -exec cat {} \;"
capture_optional "btmgmt-info-before.txt" btmgmt --index "$INDEX" info
capture_optional "btmgmt-extinfo-before.txt" btmgmt --index "$INDEX" extinfo
capture_optional "kernel-log-before.txt" journalctl -k --since "-10 min" --no-pager

BTMON_PID=""
if command_exists btmon; then
    btmon --index "$INDEX" -T >"$OUTPUT_ROOT/btmon.txt" 2>&1 &
    BTMON_PID=$!
    sleep 1
fi

cleanup() {
    if [[ -n "$BTMON_PID" ]] && kill -0 "$BTMON_PID" >/dev/null 2>&1; then
        kill -INT "$BTMON_PID" >/dev/null 2>&1 || true
        wait "$BTMON_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Power cycling through the Linux management interface causes the kernel stack
# to exercise controller initialization/HCI setup while btmon is recording.
capture_optional "btmgmt-power-off.txt" btmgmt --index "$INDEX" power off
sleep 1
capture_optional "btmgmt-power-on.txt" btmgmt --index "$INDEX" power on
sleep 2
capture_optional "btmgmt-info-after-power-cycle.txt" btmgmt --index "$INDEX" info
capture_optional "btmgmt-extinfo-after-power-cycle.txt" btmgmt --index "$INDEX" extinfo

if command_exists btmgmt; then
    capture_optional "ble-scan.txt" timeout "$((SCAN_SECONDS + 5))"         btmgmt --index "$INDEX" --timeout "$SCAN_SECONDS" find -l
    capture_optional "ble-stop-scan.txt" btmgmt --index "$INDEX" stop-find -l
fi

if [[ -n "$DEVICE" ]]; then
    record controlled_device "$DEVICE"
    record controlled_device_type "$DEVICE_TYPE"
    capture_optional "controlled-connect.txt" timeout 25         btmgmt --index "$INDEX" --timeout 20 connect -t "$DEVICE_TYPE" "$DEVICE"
    capture_optional "controlled-connection-info.txt"         btmgmt --index "$INDEX" conn-info -t "$DEVICE_TYPE" "$DEVICE"
    capture_optional "controlled-connections.txt" btmgmt --index "$INDEX" con
    capture_optional "controlled-disconnect.txt"         btmgmt --index "$INDEX" disconnect -t "$DEVICE_TYPE" "$DEVICE"
else
    record controlled_device "not-supplied"
fi

capture_optional "bluetoothctl-list-after.txt" bluetoothctl list
if [[ -n "$CONTROLLER_ADDRESS" ]]; then
    capture_optional "bluetoothctl-show.txt" bluetoothctl show "$CONTROLLER_ADDRESS"
fi
capture_optional "kernel-log-after.txt" journalctl -k --since "-10 min" --no-pager

cleanup
BTMON_PID=""
trap - EXIT INT TERM

{
    echo
    echo "PWT-005 evidence reminders:"
    echo "- verify usb-descriptors.txt shows the intended E0/01/01 single-purpose controller"
    echo "- verify driver_binding=btusb"
    echo "- inspect btmon.txt for successful HCI Reset and controller-information exchanges"
    echo "- verify controller_address is device-specific and not a shared hard-coded value"
    echo "- verify ble-scan.txt contains a controlled/known advertiser"
    echo "- when --device was supplied, verify controlled connection and disconnect succeeded"
    echo "- repeat this script after physical unplug/replug with --phase replug"
} >>"$SUMMARY"

echo "Evidence captured in $OUTPUT_ROOT"
