#!/usr/bin/env bash
set -euo pipefail

# BlueZ btmgmt has no connect command. Use its standard HCI utility to create
# a controlled LE link, explicitly selecting the peer's advertised address type.
if [[ $# != 3 || ! "$1" =~ ^hci[0-9]+$ ||
      ! "$2" =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]]; then
    echo 'Usage: pwt005-le-connect.sh hciN XX:XX:XX:XX:XX:XX le_public|le_random' >&2
    exit 2
fi
args=(-i "$1" lecc)
case "$3" in
    public|le_public) ;;
    random|le_random) args+=(--random) ;;
    *) echo 'error: invalid LE peer address type' >&2; exit 2 ;;
esac
exec timeout 30 hcitool "${args[@]}" "$2"
