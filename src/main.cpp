// PSXCore ESP32-S3 firmware entry point

#include <Arduino.h>

#include "pins.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "psx_pin_sweep.h"
#include "psx_analog_mode.h"
#include "ble_gamepad.h"
#include "psx_config.h"
#include "sd_update.h"
#include "debug_status.h"

static bool systemBooted = false;

void setup() {
  Serial.begin(115200);
  delay(500);

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

  // PSX initialization is now a two-stage boot protocol. We first try the
  // saved/default mapping. If the controller does not answer, the firmware
  // stops and runs a one-time pin sweep over GPIO 4..8.
  Serial.println("[BOOT] Initializing PSX bus...");
  psxBegin();

  uint8_t controllerId = 0;
  bool psxReady = psxProbeController(&controllerId);

  if (psxReady) {
    Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
  } else {
    Serial.println("[BOOT] PSX controller      NO RESPONSE");
    Serial.println("[BOOT] Starting pin sweep recovery...");

    if (psxPinSweep()) {
      // psxPinSweep() has already selected and persisted the working mapping.
      // Re-run the normal PSX initialization path with the corrected pins.
      Serial.println("[BOOT] Re-initializing PSX bus with corrected pins...");
      psxBegin();
      psxReady = psxProbeController(&controllerId);

      if (psxReady) {
        Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
      } else {
        Serial.println("[BOOT] PSX controller      STILL NO RESPONSE");
      }
    } else {
      Serial.println("[BOOT] Pin sweep failed");
    }
  }

  if (!psxReady) {
    Serial.println("[BOOT] PSX input disabled until a controller responds");
  }

  psxEnableAnalogMode();
  Serial.println("[BOOT] PSX analog config    OK");

  Serial.println("[BOOT] Starting Bluetooth HID...");
  bleGamepadBegin();
  Serial.println("[BOOT] Bluetooth HID        OK");
  Serial.println("[BOOT] BLE advertising      ON");

  systemBooted = true;
  Serial.println("================================");
  Serial.println("[BOOT] PSXCore READY");
  Serial.printf("[BOOT] PSX polling          %s\n", psxReady ? "AVAILABLE" : "DISABLED");
  Serial.println("================================");
}

void loop() {
  // PSX polling remains deliberately stopped while the boot protocol is being
  // validated. This prevents the old FF/FF transaction stream from returning.
  if (!systemBooted) {
    return;
  }

  bleGamepadUpdate();
  debugStatusLoop();
  delay(5);
}
