#include <Arduino.h>
#include "psx_protocol.h"
#include "pins.h"

void psxProtocolInit() {
  pinMode(PSX_DATA, INPUT_PULLUP);
  pinMode(PSX_COMMAND, OUTPUT);
  pinMode(PSX_ATTENTION, OUTPUT);
  pinMode(PSX_CLOCK, OUTPUT);
  pinMode(PSX_ACK, INPUT_PULLUP);
  digitalWrite(PSX_ATTENTION, HIGH);
  digitalWrite(PSX_CLOCK, HIGH);
}

uint8_t psxTransferByte(uint8_t value) {
  uint8_t result = 0;

  for (int bit = 0; bit < 8; bit++) {
    digitalWrite(PSX_COMMAND, (value >> bit) & 1);
    digitalWrite(PSX_CLOCK, LOW);
    delayMicroseconds(2);

    if (digitalRead(PSX_DATA)) {
      result |= (1 << bit);
    }

    digitalWrite(PSX_CLOCK, HIGH);
    delayMicroseconds(2);
  }

  return result;
}
