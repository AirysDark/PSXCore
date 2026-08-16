#include <Arduino.h>
#include "psx_protocol.h"
#include "pins.h"

static bool psxLastAck = false;

void psxProtocolInit() {
  pinMode(PSX_DATA, INPUT_PULLUP);
  pinMode(PSX_COMMAND, OUTPUT);
  pinMode(PSX_ATTENTION, OUTPUT);
  pinMode(PSX_CLOCK, OUTPUT);
  pinMode(PSX_ACK, INPUT_PULLUP);

  // PSX bus idle state.
  digitalWrite(PSX_COMMAND, HIGH);
  digitalWrite(PSX_ATTENTION, HIGH);
  digitalWrite(PSX_CLOCK, HIGH);
}

uint8_t psxTransferByte(uint8_t value) {
  uint8_t result = 0;

  // PSX serial timing is LSB first. The controller changes DATA while the
  // clock is low; sample it before returning the clock high.
  for (int bit = 0; bit < 8; bit++) {
    digitalWrite(PSX_COMMAND, (value >> bit) & 1);
    delayMicroseconds(1);

    digitalWrite(PSX_CLOCK, LOW);
    delayMicroseconds(2);

    if (digitalRead(PSX_DATA)) {
      result |= (1 << bit);
    }

    digitalWrite(PSX_CLOCK, HIGH);
    delayMicroseconds(2);
  }

  // A responding PSX controller pulls ACK low after each byte. Never wait
  // forever: a disconnected controller must not stall the firmware loop.
  psxLastAck = false;
  const uint32_t start = micros();
  while ((micros() - start) < 250) {
    if (digitalRead(PSX_ACK) == LOW) {
      psxLastAck = true;
      break;
    }
  }

  // Allow the controller to release ACK before the next byte.
  if (psxLastAck) {
    const uint32_t ackStart = micros();
    while ((micros() - ackStart) < 250 && digitalRead(PSX_ACK) == LOW) {
      delayMicroseconds(1);
    }
  }

  return result;
}

bool psxLastTransferAcked() {
  return psxLastAck;
}
