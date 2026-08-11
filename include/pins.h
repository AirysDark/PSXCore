#pragma once

// PSX/PS2 controller interface pins
#define PSX_DATA      4
#define PSX_COMMAND   5
#define PSX_ATTENTION 6
#define PSX_CLOCK     7
#define PSX_ACK       8

// SD card firmware updater pins
// Change these if your ESP32-S3 Tiny board uses different wiring
#define SD_CS         10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12

// Future ESP32-S3 BLE HID configuration goes here
