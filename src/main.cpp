// PSXCore ESP32-S3 firmware entry point

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "ble_gamepad.h"
#include "psx_config.h"

void setup() {
  psxBegin();
  psx_enable_analog_mode();
  bleGamepadBegin();
}

void loop() {
  psxReadController();
  bleGamepadUpdate();
}
