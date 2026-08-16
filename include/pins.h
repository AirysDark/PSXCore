#pragma once

#include <stdint.h>

// PSX/PS2 controller interface pins.
// Defaults match the ESP32-S3 Tiny / full-size ESP32-S3 wiring currently used.
#define PSX_DEFAULT_DATA      4
#define PSX_DEFAULT_COMMAND   5
#define PSX_DEFAULT_ATTENTION 6
#define PSX_DEFAULT_CLOCK     7
#define PSX_DEFAULT_ACK       8

struct PsxPins {
  int data;
  int command;
  int attention;
  int clock;
  int ack;
};

extern PsxPins psxPins;

// Load the saved pin mapping, falling back to 4/5/6/7/8.
void psxPinsBegin();

// Replace the active mapping and persist it for the next boot.
void psxSetPins(const PsxPins& pins, bool persist = true);

// Restore the factory/default mapping and erase the saved mapping.
void psxResetPins();

// Runtime pin aliases. Existing PSX protocol/configuration code can continue
// using the legacy names while automatically following the active pin mapping.
#define PSX_DATA      (psxPins.data)
#define PSX_COMMAND   (psxPins.command)
#define PSX_ATTENTION (psxPins.attention)
#define PSX_CLOCK     (psxPins.clock)
#define PSX_ACK       (psxPins.ack)

// SD card firmware updater pins.
#define SD_CS         10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12
