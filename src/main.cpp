// PSXCore ESP32-S3 firmware entry point

#include "pins.h"

void psxBegin();
void psxReadController();
void bleGamepadBegin();
void bleGamepadUpdate();
bool psx_enable_analog_mode();

void setup() {
  psxBegin();
  psx_enable_analog_mode();
  bleGamepadBegin();
}

void loop() {
  psxReadController();
  bleGamepadUpdate();
}
