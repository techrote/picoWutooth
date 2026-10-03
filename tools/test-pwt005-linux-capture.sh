#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/bin"
printf 'fixture-uf2\n' > "$scratch/candidate.uf2"
expected="$(sha256sum "$scratch/candidate.uf2" | awk '{print $1}')"
cat > "$scratch/bin/git" <<'EOF'
#!/usr/bin/env bash
case "$*" in
    'rev-parse --show-toplevel') echo /fixture ;;
    'rev-parse HEAD') echo 0123456789abcdef0123456789abcdef01234567 ;;
    'status --short --branch') echo '## fixture' ;;
    *) exit 2 ;;
esac
EOF
cat > "$scratch/bin/uname" <<'EOF'
#!/usr/bin/env bash
echo Linux
EOF
for cmd in lsusb btmgmt btmon bluetoothctl journalctl; do
    cat > "$scratch/bin/$cmd" <<'EOF'
#!/usr/bin/env bash
echo 'fixture command output'
exit 42
EOF
done
chmod +x "$scratch/bin/"*
# Force a nonexistent HCI node: no physical host controller is used by this test.
set +e
PATH="$scratch/bin:$PATH" bash "$root/tools/pwt005-linux-capture.sh" \
    --uf2 "$scratch/candidate.uf2" --hci hci-pwt005-test-missing \
    --output "$scratch/result" > "$scratch/stdout" 2>&1
status=$?
set -e
[[ "$status" == 3 ]] || { cat "$scratch/stdout"; echo "expected missing-HCI status 3, got $status" >&2; exit 1; }
grep -qx "uf2_sha256=$expected" "$scratch/result/summary.txt"
grep -qx 'repository_commit=0123456789abcdef0123456789abcdef01234567' "$scratch/result/summary.txt"
grep -qx 'hci_device=NOT_FOUND' "$scratch/result/summary.txt"
grep -qx 'driver_binding=NOT_FOUND' "$scratch/result/summary.txt"
grep -qx 'exit_status=0' "$scratch/result/git-status.txt"
for file in usb-device usb-descriptors usb-tree bluetoothctl-list kernel-log; do
    grep -qx 'fixture command output' "$scratch/result/$file.txt"
    grep -qx 'exit_status=42' "$scratch/result/$file.txt"
done
echo 'PWT-005 capture continues after successful/failed commands and preserves evidence'

cat > "$scratch/bin/hcitool" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" > "$PWT_TEST_ARGS"
exit 17
EOF
chmod +x "$scratch/bin/hcitool"
for type in le_public le_random; do
    set +e
    PWT_TEST_ARGS="$scratch/connect-args" PATH="$scratch/bin:$PATH" \
        bash "$root/tools/pwt005-le-connect.sh" hci3 12:34:56:78:9A:BC "$type"
    status=$?
    set -e
    [[ "$status" == 17 ]] # Preserve actual connection-tool failure.
    if [[ "$type" == le_random ]]; then
        grep -qx -- '-i hci3 lecc --random 12:34:56:78:9A:BC' "$scratch/connect-args"
    else
        grep -qx -- '-i hci3 lecc 12:34:56:78:9A:BC' "$scratch/connect-args"
    fi
done
echo 'PWT-005 controlled LE connect preserves peer address type and failure status'
