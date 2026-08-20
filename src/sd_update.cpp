#include <Arduino.h>
#include <SD.h>
#include <Update.h>
#include "sd_update.h"
#include "pins.h"

// SD firmware update file:
// /firmware/PSXCore.bin
// No config files required.
//
// SD support is disabled until the external SD module is actually wired.
// Enable it with -DPSXCORE_SD_ENABLED=1 in PlatformIO build_flags.
#ifndef PSXCORE_SD_ENABLED
#define PSXCORE_SD_ENABLED 0
#endif

bool sdUpdateCheck() {
#if !PSXCORE_SD_ENABLED
  Serial.println("BOOT: SD updater disabled (SD hardware not configured)");
  return false;
#else
  Serial.println("BOOT: checking SD updater");

  if (!SD.begin(SD_CS)) {
    Serial.println("SD: not detected");
    return false;
  }

  Serial.println("SD: detected");

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

  if (size < 1024) {
    Serial.println("OTA: invalid firmware size");
    firmware.close();
    return false;
  }

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

  SD.remove(path);

  Serial.println("OTA: update complete");
  Serial.println("BOOT: restarting into new firmware");
  delay(1000);
  ESP.restart();

  return true;
#endif
}
