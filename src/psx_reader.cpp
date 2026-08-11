// PSXCore ESP32-S3 PS2 controller reader
// Reads the original PSX controller bus signals.

#include "pins.h"

volatile uint8_t psxButtons[2];
volatile uint8_t psxAnalog[4];

void psxBegin() {
  pinMode(PSX_DATA, INPUT_PULLUP);
  pinMode(PSX_COMMAND, OUTPUT);
  pinMode(PSX_ATTENTION, INPUT_PULLUP);
  pinMode(PSX_CLOCK, INPUT_PULLUP);
  pinMode(PSX_ACK, INPUT_PULLUP);
}

// Placeholder protocol engine.
// Full implementation will bit-bang the PSX synchronous serial protocol.
void psxReadController() {
  // TODO: implement clock synchronized reads
  // TODO: decode digital buttons
  // TODO: decode analog sticks
}
