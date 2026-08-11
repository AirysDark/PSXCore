#include <Arduino.h>
#include <SD.h>
#include "sd_update.h"

// SD update service
// Update file location:
// /firmware/PSXCore.bin
// No config files or folders are required.

bool sdUpdateCheck() {
  if (!SD.begin()) {
    Serial.println("SD: not detected");
    return false;
  }

  if (!SD.exists("/firmware/PSXCore.bin")) {
    Serial.println("SD: no firmware update");
    return false;
  }

  Serial.println("SD: firmware update found");
  return true;
}
