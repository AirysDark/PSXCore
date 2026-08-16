#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "controller_state.h"
#include "pins.h"

// PSX command transaction engine
// Uses the same runtime pin mapping as the main PSX protocol engine.

static uint8_t transferByte(uint8_t data) {
  uint8_t result = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(psxPins.command, (data >> i) & 1);
    digitalWrite(psxPins.clock, LOW);
    delayMicroseconds(2);
    if (digitalRead(psxPins.data)) result |= (1 << i);
    digitalWrite(psxPins.clock, HIGH);
    delayMicroseconds(2);
  }
  return result;
}

bool psxPoll(uint8_t *packet, size_t length) {
  digitalWrite(psxPins.attention, LOW);

  transferByte(0x01);
  transferByte(0x42);
  transferByte(0x00);

  for (size_t i = 0; i < length; i++) {
    packet[i] = transferByte(0x00);
  }

  digitalWrite(psxPins.attention, HIGH);
  return true;
}
