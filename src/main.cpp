// PSXCore ESP32-S3 firmware entry point

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "ble_gamepad.h"
#include "psx_config.h"
#include "sd_update.h"

void setup() {
  Serial.begin(115200);

  // Check SD card before normal startup.
  // If no update exists, continue boot normally.
  sdUpdateCheck();

  psxBegin();
  psx_enable_analog_mode();
  bleGamepadBegin();
}

void loop() {
  psxReadController();
  bleGamepadUpdate();
}
