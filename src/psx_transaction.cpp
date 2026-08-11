#include "controller_state.h"
#include "pins.h"

// PSX command transaction engine
// Implements the standard controller poll sequence foundation.

static uint8_t transferByte(uint8_t data) {
  uint8_t result = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_COMMAND, (data >> i) & 1);
    digitalWrite(PIN_CLOCK, LOW);
    delayMicroseconds(2);
    if (digitalRead(PIN_DATA)) result |= (1 << i);
    digitalWrite(PIN_CLOCK, HIGH);
    delayMicroseconds(2);
  }
  return result;
}

bool psxPoll(uint8_t *packet, size_t length) {
  digitalWrite(PIN_ATTENTION, LOW);

  transferByte(0x01);
  transferByte(0x42);
  transferByte(0x00);

  for (size_t i = 0; i < length; i++) {
    packet[i] = transferByte(0x00);
  }

  digitalWrite(PIN_ATTENTION, HIGH);
  return true;
}
