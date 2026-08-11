#include <Arduino.h>
#include <SD.h>
#include "sd_update.h"

// Placeholder SD update service.
// Future version: verify version.txt, then stream firmware.bin to OTA partition.

bool sdUpdateCheck() {
  if (!SD.begin()) {
    Serial.println("SD: not detected");
    return false;
  }

  if (!SD.exists("/firmware/PSXCore.bin")) {
    Serial.println("SD: no update found");
    return false;
  }

  Serial.println("SD: update package found");
  Serial.println("SD update handler ready");
  return false;
}
