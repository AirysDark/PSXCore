#include <Arduino.h>
#include "psx_protocol.h"
#include "pins.h"

static bool psxLastAck = false;

void psxProtocolInit() {
  pinMode(psxPins.data, INPUT_PULLUP);
  pinMode(psxPins.command, OUTPUT);
  pinMode(psxPins.attention, OUTPUT);
  pinMode(psxPins.clock, OUTPUT);
  pinMode(psxPins.ack, INPUT_PULLUP);

  // PSX bus idle state.
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.clock, HIGH);
}

uint8_t psxTransferByte(uint8_t value) {
  uint8_t result = 0;

  // PSX serial timing is LSB first. The controller changes DATA while the
  // clock is low; sample it before returning the clock high.
  for (int bit = 0; bit < 8; bit++) {
    digitalWrite(psxPins.command, (value >> bit) & 1);
    delayMicroseconds(1);

    digitalWrite(psxPins.clock, LOW);
    delayMicroseconds(2);

    if (digitalRead(psxPins.data)) {
      result |= (1 << bit);
    }

    digitalWrite(psxPins.clock, HIGH);
    delayMicroseconds(2);
  }

  // A responding PSX controller pulls ACK low after each byte. Never wait
  // forever: a disconnected controller must not stall the firmware loop.
  psxLastAck = false;
  const uint32_t start = micros();
  while ((micros() - start) < 250) {
    if (digitalRead(psxPins.ack) == LOW) {
      psxLastAck = true;
      break;
    }
  }

  // Allow the controller to release ACK before the next byte.
  if (psxLastAck) {
    const uint32_t ackStart = micros();
    while ((micros() - ackStart) < 250 && digitalRead(psxPins.ack) == LOW) {
      delayMicroseconds(1);
    }
  }

  return result;
}

bool psxLastTransferAcked() {
  return psxLastAck;
}
