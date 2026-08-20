#pragma once

#include <stdint.h>

// Proven PS2/PSX controller wiring used by the current full-size ESP32-S3
// test board and the ESP32-S3 Tiny target.
#define PSX_DEFAULT_DATA      6
#define PSX_DEFAULT_COMMAND   4
#define PSX_DEFAULT_ATTENTION 7
#define PSX_DEFAULT_CLOCK     5
#define PSX_DEFAULT_ACK       8

struct PsxPins {
  int data;
  int command;
  int attention;
  int clock;
  int ack;
};

extern PsxPins psxPins;

// Load the saved pin mapping, falling back to the proven 6/4/7/5/8 mapping.
void psxPinsBegin();

// Replace the active mapping and persist it for the next boot.
void psxSetPins(const PsxPins& pins, bool persist = true);

// Restore the factory/default mapping and erase the saved mapping.
void psxResetPins();

// Runtime pin aliases. Existing PSX protocol/configuration code can continue
// using the legacy names while automatically following the active mapping.
#define PSX_DATA      (psxPins.data)
#define PSX_COMMAND   (psxPins.command)
#define PSX_ATTENTION (psxPins.attention)
#define PSX_CLOCK     (psxPins.clock)
#define PSX_ACK       (psxPins.ack)

// SD card firmware updater pins. These remain unused until SD hardware is
// explicitly enabled/configured.
#define SD_CS         10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12
