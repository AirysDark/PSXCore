#include <Arduino.h>
#include "pins.h"
#include "controller_state.h"

void psxcoreBegin() {
  psxPinsBegin();

  pinMode(psxPins.data, INPUT_PULLUP);
  pinMode(psxPins.command, OUTPUT);
  pinMode(psxPins.attention, OUTPUT);
  pinMode(psxPins.clock, OUTPUT);
  pinMode(psxPins.ack, INPUT_PULLUP);

  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.clock, HIGH);
}
