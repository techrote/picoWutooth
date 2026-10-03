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

Planning/bootstrap. No firmware implementation has been accepted yet.

The MVP targets a single-purpose USB Bluetooth HCI device, with Linux used first for protocol-level diagnostics and Windows 11 as the primary generic-dongle acceptance target. BLE is the first functional milestone; BR/EDR follows. Wi-Fi coexistence, USB composite debug interfaces, and SCO/ISO audio are deliberately deferred until the basic controller path is stable.

Start with [RAG.md](RAG.md) once present. It is the retrieval/index document for the programme and defines the authoritative read order for implementation issues.
