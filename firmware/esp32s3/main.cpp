// PSXCore ESP32-S3 firmware entry point

#include "pins.h"

void psxBegin();
void psxReadController();
void bleGamepadBegin();
void bleGamepadUpdate();

void setup() {
  psxBegin();
  bleGamepadBegin();
}

void loop() {
  psxReadController();
  bleGamepadUpdate();
}
