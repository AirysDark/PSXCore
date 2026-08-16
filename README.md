# PSXCore

ESP32-S3 based PlayStation controller core project.

## Goals

- Replace the original PSX/PS2 cable with ESP32-S3
- Read controller inputs
- Provide Bluetooth HID gamepad support
- Support PS2 controller hardware conversion

## Supported ESP32-S3 boards

PSXCore has dedicated PlatformIO environments for both target boards:

| Environment | Flash | PSRAM | Partition table |
|---|---:|---:|---|
| `esp32-s3` | 8 MB | 8 MB OPI | `partitions.csv` |
| `esp32-s3-tiny` | 4 MB | 2 MB QSPI | `partitions_tiny.csv` |

Build the full-size board with:

```text
pio run -e esp32-s3
```

Build the ESP32-S3 Tiny FH4R2 with:

```text
pio run -e esp32-s3-tiny
```

The partition layouts reserve NVS for persistent settings, provide dual OTA application slots, and provide a LittleFS data area. The Tiny layout is sized specifically to stay within its 4 MB flash.

## PSX controller boot protocol

Default PSX wiring is:

| Role | GPIO |
|---|---:|
| DATA | 4 |
| COMMAND | 5 |
| ATTENTION | 6 |
| CLOCK | 7 |
| ACK | 8 |

At boot the firmware initializes the PSX bus and probes for a controller. If there is no valid response, it enters **pin sweep recovery** and tests permutations of GPIO 4, 5, 6, 7 and 8. A valid controller response causes the corrected mapping to be saved in NVS and the PSX bus to be initialized again.

PSX polling remains disabled when no controller is detected, preventing the previous continuous `FF` transaction output from flooding the serial monitor.

## Persistent pin mapping

Corrected PSX pin mappings are stored in the `psxcore` NVS namespace. A fresh device automatically creates the namespace without treating the missing namespace as an error.

## SD updater

The boot sequence checks the SD card for the firmware update before starting the PSX bus and Bluetooth HID system. If no card is detected, normal boot continues.

## Bluetooth HID

When the PSX side is ready, PSXCore starts Bluetooth HID gamepad advertising so the converted controller can be used as a wireless gamepad.
