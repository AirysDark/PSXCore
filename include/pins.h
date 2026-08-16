#pragma once

// PSX/PS2 controller interface pins
#define PSX_DATA      4
#define PSX_COMMAND   5
#define PSX_ATTENTION 6
#define PSX_CLOCK     7
#define PSX_ACK       8

// Compatibility aliases used by PSX transaction engine
#define PIN_DATA       PSX_DATA
#define PIN_COMMAND    PSX_COMMAND
#define PIN_ATTENTION  PSX_ATTENTION
#define PIN_CLOCK      PSX_CLOCK
#define PIN_ACK        PSX_ACK

// SD card firmware updater pins
// Change these if your ESP32-S3 Tiny board uses different wiring
#define SD_CS         10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12

// Future ESP32-S3 BLE HID configuration goes here
