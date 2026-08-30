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

Proven ESP32-S3 signal wiring is:

| Role | GPIO |
|---|---:|
| DATA | 6 |
| COMMAND | 4 |
| ATTENTION | 7 |
| CLOCK | 5 |
| ACK | 8 |

At boot the firmware initializes the PSX bus and probes for a controller. If there is no valid response, it enters **pin sweep recovery** and tests permutations of GPIO 4, 5, 6, 7 and 8. A valid controller response causes the corrected mapping to be saved in NVS and the PSX bus to be initialized again.

PSX polling remains disabled when no controller is detected, preventing the previous continuous `FF` transaction output from flooding the serial monitor.

## Full PS2 controller connector

PSXCore now keeps the complete original 9-pin controller connector definition, including the original rumble supply and unused line.

| Connector pin | Role | PSXCore use |
|---:|---|---|
| 1 | DATA | ESP32 signal |
| 2 | COMMAND | ESP32 signal |
| 3 | ~7V RUMBLE | Reserved/original motor supply |
| 4 | GND | Ground |
| 5 | 3.3V | Controller logic supply |
| 6 | ATTENTION | ESP32 signal |
| 7 | CLOCK | ESP32 signal |
| 8 | NC | Reserved / not connected |
| 9 | ACK | ESP32 signal |

The original ~7V rumble line must **not** be connected directly to an ESP32 GPIO. It is retained in the connector definition for hardware compatibility and future power-driver work.

A separate `PSX_RUMBLE_3V_GPIO` definition is reserved for the planned 3V rumble implementation. It is currently `-1` (disabled) until the final GPIO and external transistor/MOSFET motor driver are selected.

## Persistent pin mapping

Corrected PSX pin mappings are stored in the `psxcore` NVS namespace. A fresh device automatically creates the namespace without treating the missing namespace as an error.

## SD updater

The boot sequence checks the SD card for the firmware update before starting the PSX bus and Bluetooth HID system. If no card is detected, normal boot continues.

## Bluetooth HID

When the PSX side is ready, PSXCore starts Bluetooth HID gamepad advertising so the converted controller can be used as a wireless gamepad.
