#include "pins.h"

// PSX protocol engine foundation
// Implements the low level PS2 controller bus layer.

void psxProtocolInit() {
  pinMode(PSX_DATA, INPUT_PULLUP);
  pinMode(PSX_COMMAND, OUTPUT);
  pinMode(PSX_ATTENTION, OUTPUT);
  pinMode(PSX_CLOCK, OUTPUT);
  pinMode(PSX_ACK, INPUT_PULLUP);
}

uint8_t psxTransferByte(uint8_t value) {
  // TODO: clock accurate bit transfer
  // PSX uses LSB first serial communication.
  return 0;
}
