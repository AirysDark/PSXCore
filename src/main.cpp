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

static bool systemBooted = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Keep boot output short and deterministic. Controller polling is disabled
  // after boot until the PSX bring-up is deliberately enabled again.
  Serial.println();
  Serial.println("================================");
  Serial.println("          PSXCore BOOT");
  Serial.println("================================");

  debugStatusInit();
  Serial.println("[BOOT] Debug system      OK");

  Serial.println("[BOOT] Checking SD update...");
  if (sdUpdateCheck()) {
    Serial.println("[BOOT] SD update handled");
  } else {
    Serial.println("[BOOT] No SD update");
  }

  Serial.println("[BOOT] Initializing PSX bus...");
  psxBegin();
  Serial.println("[BOOT] PSX bus             OK");

  // Analog-mode configuration is currently a stub, so call it only as part
  // of the boot sequence. It does not poll the controller.
  psxEnableAnalogMode();
  Serial.println("[BOOT] PSX analog config    OK");

  Serial.println("[BOOT] Starting Bluetooth HID...");
  bleGamepadBegin();
  Serial.println("[BOOT] Bluetooth HID        OK");
  Serial.println("[BOOT] BLE advertising      ON");

  systemBooted = true;
  Serial.println("================================");
  Serial.println("[BOOT] PSXCore READY");
  Serial.println("[BOOT] PSX polling          STOPPED");
  Serial.println("================================");
}

void loop() {
  // STOP PSX input polling for now. This deliberately prevents the FF/FF
  // transaction stream from hammering the controller and serial monitor
  // while we work on the boot/initialization sequence.
  if (!systemBooted) {
    return;
  }

  // BLE remains alive, but there is no controller polling yet.
  bleGamepadUpdate();

  // Do not print continuously. The normal status system can still run at its
  // own controlled interval if enabled by the implementation.
  debugStatusLoop();

  delay(5);
}
