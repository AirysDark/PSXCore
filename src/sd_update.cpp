#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>

#include "sd_update.h"
#include "pins.h"

// SD firmware update file:
// /firmware/PSXCore.bin
//
// SD wiring:
// CS   = GPIO10
// MOSI = GPIO11
// SCK  = GPIO12
// MISO = GPIO13

#ifndef PSXCORE_SD_ENABLED
#define PSXCORE_SD_ENABLED 0
#endif

static SPIClass sdSPI(FSPI);

bool sdUpdateCheck() {
#if !PSXCORE_SD_ENABLED
  Serial.println("BOOT: SD updater disabled");
  return false;
#else
  Serial.println("SD: initializing SPI...");

  // Explicitly initialize SPI with the PSXCore SD card wiring.
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  Serial.printf(
    "SD: CS=%d MOSI=%d MISO=%d SCK=%d\n",
    SD_CS,
    SD_MOSI,
    SD_MISO,
    SD_SCK
  );

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD: card not detected or mount failed");
    return false;
  }

  Serial.println("SD: card detected");

  const char *path = "/firmware/PSXCore.bin";

  if (!SD.exists(path)) {
    Serial.println("SD: no firmware update found");
    SD.end();
    return false;
  }

  File firmware = SD.open(path, FILE_READ);

  if (!firmware) {
    Serial.println("SD: failed to open firmware");
    SD.end();
    return false;
  }

  size_t size = firmware.size();

  Serial.printf(
    "SD: firmware found: %s (%u bytes)\n",
    path,
    static_cast<unsigned>(size)
  );

  if (size < 1024) {
    Serial.println("OTA: firmware file too small");
    firmware.close();
    SD.end();
    return false;
  }

  Serial.println("OTA: starting update...");

  if (!Update.begin(size)) {
    Serial.printf("OTA: not enough space, error=%u\n", Update.getError());
    firmware.close();
    SD.end();
    return false;
  }

  size_t written = Update.writeStream(firmware);
  firmware.close();

  Serial.printf(
    "OTA: written %u / %u bytes\n",
    static_cast<unsigned>(written),
    static_cast<unsigned>(size)
  );

  if (written != size) {
    Serial.printf("OTA: write failed, error=%u\n", Update.getError());
    Update.abort();
    SD.end();
    return false;
  }

  if (!Update.end(true)) {
    Serial.printf("OTA: finalize failed, error=%u\n", Update.getError());
    SD.end();
    return false;
  }

  Serial.println("OTA: update verified successfully");

  // Remove the update file so it cannot run again on the next boot.
  if (SD.remove(path)) {
    Serial.println("SD: update file removed");
  } else {
    Serial.println("SD: WARNING - could not remove update file");
  }

  SD.end();

  Serial.println("OTA: update complete");
  Serial.println("BOOT: restarting into new firmware...");
  delay(1000);

  ESP.restart();
  return true;
#endif
}
