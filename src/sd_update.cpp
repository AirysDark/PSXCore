#include <Arduino.h>
#include <SD.h>
#include <Update.h>
#include "sd_update.h"

// SD firmware update file:
// /firmware/PSXCore.bin
// No config files required.

bool sdUpdateCheck() {
  if (!SD.begin()) {
    Serial.println("SD: not detected");
    return false;
  }

  const char *path = "/firmware/PSXCore.bin";

  if (!SD.exists(path)) {
    Serial.println("SD: no firmware update");
    return false;
  }

  File firmware = SD.open(path, FILE_READ);
  if (!firmware) {
    Serial.println("SD: failed opening firmware");
    return false;
  }

  size_t size = firmware.size();
  Serial.printf("SD: firmware found (%u bytes)\n", (unsigned)size);

  if (!Update.begin(size)) {
    Serial.println("OTA: not enough space");
    firmware.close();
    return false;
  }

  size_t written = Update.writeStream(firmware);
  firmware.close();

  if (written != size) {
    Serial.println("OTA: write failed");
    Update.abort();
    return false;
  }

  if (!Update.end(true)) {
    Serial.println("OTA: finalize failed");
    return false;
  }

  Serial.println("OTA: update complete, restarting");
  delay(1000);
  ESP.restart();

  return true;
}
