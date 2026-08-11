// PSXCore ESP32-S3 firmware entry point

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "ble_gamepad.h"
#include "psx_config.h"
#include "sd_update.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("PSXCore boot");
  Serial.println("Checking SD updater");

  // Check SD card before normal startup.
  // If no update exists, continue boot normally.
  if (!sdUpdateCheck()) {
    Serial.println("SD update skipped");
  }

  Serial.println("Starting PSX controller");
  psxBegin();
  psx_enable_analog_mode();
  Serial.println("PSX ready");

  Serial.println("Starting Bluetooth HID");
  bleGamepadBegin();
  Serial.println("BLE advertising started");
}

void loop() {
  psxReadController();
  bleGamepadUpdate();
}
