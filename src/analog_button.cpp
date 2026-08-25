#include <Arduino.h>
#include "analog_button.h"

static bool initialized = false;
static bool analogMode = false;
static AnalogButtonEvent pendingEvent = AnalogButtonEvent::None;

static bool isAnalogId(uint8_t controllerId) {
  return controllerId == 0x73 || controllerId == 0x79;
}

void analogButtonInit(uint8_t initialControllerId) {
  analogMode = isAnalogId(initialControllerId);
  pendingEvent = AnalogButtonEvent::None;
  initialized = true;

  Serial.printf("[ANALOG] Initial mode: %s (ID=%02X)\n",
                analogMode ? "ANALOG" : "DIGITAL",
                initialControllerId);
}

void analogButtonUpdate(uint8_t controllerId) {
  const bool newAnalogMode = isAnalogId(controllerId);

  if (!initialized) {
    analogButtonInit(controllerId);
    return;
  }

  if (newAnalogMode == analogMode) return;

  analogMode = newAnalogMode;
  pendingEvent = AnalogButtonEvent::ShortPress;

  Serial.printf("[ANALOG] Button detected: mode -> %s (ID=%02X)\n",
                analogMode ? "ANALOG" : "DIGITAL",
                controllerId);
}

AnalogButtonEvent analogButtonTakeEvent() {
  const AnalogButtonEvent event = pendingEvent;
  pendingEvent = AnalogButtonEvent::None;
  return event;
}

bool analogButtonIsAnalogMode() {
  return analogMode;
}

bool analogButtonWasPressed() {
  if (pendingEvent == AnalogButtonEvent::None) return false;
  pendingEvent = AnalogButtonEvent::None;
  return true;
}

bool analogButtonLongPressAvailable() {
  return false;
}
