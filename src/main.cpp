// PSXCore ESP32-S3 firmware entry point

#include <Arduino.h>

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "psx_analog_mode.h"
#include "ble_gamepad.h"
#include "psx_config.h"
#include "sd_update.h"
#include "debug_status.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  debugStatusInit();

  Serial.println("PSXCore boot");
  Serial.println("Checking SD updater");

  if (!sdUpdateCheck()) {
    Serial.println("SD update skipped");
  }

  Serial.println("Starting PSX controller");
  psxBegin();
  psxEnableAnalogMode();
  Serial.println("PSX ready");

  Serial.println("Starting Bluetooth HID");
  bleGamepadBegin();
  Serial.println("BLE advertising started");
}

void loop() {
  psxReadController();
  debugStatusPSXPacket();

  bleGamepadUpdate();
  debugStatusBLEUpdate();

  debugStatusLoop();
}
