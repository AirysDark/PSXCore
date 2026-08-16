// PSXCore ESP32-S3 firmware entry point

#include <Arduino.h>

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "psx_analog_mode.h"
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
  psxEnableAnalogMode();
  Serial.println("PSX ready");

  Serial.println("Starting Bluetooth HID");
  bleGamepadBegin();
  Serial.println("BLE advertising started");
}

void loop() {
  static uint32_t loopCount = 0;
  static uint32_t psxUpdateCount = 0;
  static uint32_t bleUpdateCount = 0;

  loopCount++;

  psxReadController();
  psxUpdateCount++;

  bleGamepadUpdate();
  bleUpdateCount++;

  // Runtime diagnostics so hangs or failed subsystems are visible.
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug >= 1000) {
    lastDebug = millis();

    Serial.print("[PSXCore] uptime=");
    Serial.print(millis() / 1000);
    Serial.print("s loop=");
    Serial.print(loopCount);
    Serial.print(" psx=");
    Serial.print(psxUpdateCount);
    Serial.print(" ble=");
    Serial.println(bleUpdateCount);
  }
}
