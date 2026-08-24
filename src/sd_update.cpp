#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>

#include "sd_update.h"
#include "pins.h"
#include "version.h"

// SD update layout:
// /firmware/PSXCore.bin
// /firmware/version.txt

#ifndef PSXCORE_SD_ENABLED
#define PSXCORE_SD_ENABLED 0
#endif

static SPIClass sdSPI(FSPI);

static bool parseVersion(const String &text, int &major, int &minor, int &patch) {
  String version = text;
  version.trim();

  if (sscanf(version.c_str(), "%d.%d.%d", &major, &minor, &patch) != 3) {
    return false;
  }

  if (major < 0 || minor < 0 || patch < 0) {
    return false;
  }

  return true;
}

static int compareVersions(const String &installed, const String &candidate) {
  int installedMajor, installedMinor, installedPatch;
  int candidateMajor, candidateMinor, candidatePatch;

  if (!parseVersion(installed, installedMajor, installedMinor, installedPatch) ||
      !parseVersion(candidate, candidateMajor, candidateMinor, candidatePatch)) {
    return -2; // Invalid version format.
  }

  if (candidateMajor != installedMajor) {
    return candidateMajor > installedMajor ? 1 : -1;
  }

  if (candidateMinor != installedMinor) {
    return candidateMinor > installedMinor ? 1 : -1;
  }

  if (candidatePatch != installedPatch) {
    return candidatePatch > installedPatch ? 1 : -1;
  }

  return 0;
}

bool sdUpdateCheck() {
#if !PSXCORE_SD_ENABLED
  Serial.println("BOOT: SD updater disabled");
  return false;
#else
  const char *firmwarePath = "/firmware/PSXCore.bin";
  const char *versionPath = "/firmware/version.txt";

  Serial.printf("[BOOT] Installed version   %s\n", PSXCORE_VERSION_STRING);
  Serial.println("SD: initializing SPI...");

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  Serial.printf("SD: CS=%d MOSI=%d MISO=%d SCK=%d\n",
                SD_CS, SD_MOSI, SD_MISO, SD_SCK);

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD: card not detected or mount failed");
    return false;
  }

  Serial.println("SD: card detected");

  if (!SD.exists(versionPath)) {
    Serial.println("SD: no version.txt - skipping update");
    SD.end();
    return false;
  }

  File versionFile = SD.open(versionPath, FILE_READ);
  if (!versionFile) {
    Serial.println("SD: failed to open version.txt");
    SD.end();
    return false;
  }

  String candidateVersion = versionFile.readString();
  versionFile.close();
  candidateVersion.trim();

  Serial.printf("SD: update version      %s\n", candidateVersion.c_str());

  const int comparison = compareVersions(PSXCORE_VERSION_STRING, candidateVersion);

  if (comparison == -2) {
    Serial.println("SD: invalid version format - expected X.Y.Z");
    SD.end();
    return false;
  }

  if (comparison == 0) {
    Serial.println("SD: same version - skipping update");
    SD.end();
    return false;
  }

  if (comparison < 0) {
    Serial.println("SD: installed firmware is newer - skipping update");
    SD.end();
    return false;
  }

  Serial.println("SD: newer firmware detected");

  if (!SD.exists(firmwarePath)) {
    Serial.println("SD: version is newer but PSXCore.bin is missing");
    SD.end();
    return false;
  }

  File firmware = SD.open(firmwarePath, FILE_READ);
  if (!firmware) {
    Serial.println("SD: failed to open firmware");
    SD.end();
    return false;
  }

  const size_t size = firmware.size();
  Serial.printf("SD: firmware found: %s (%u bytes)\n",
                firmwarePath, static_cast<unsigned>(size));

  if (size < 1024) {
    Serial.println("OTA: firmware file too small");
    firmware.close();
    SD.end();
    return false;
  }

  Serial.printf("OTA: updating %s -> %s\n",
                PSXCORE_VERSION_STRING, candidateVersion.c_str());

  if (!Update.begin(size)) {
    Serial.printf("OTA: not enough space, error=%u\n", Update.getError());
    firmware.close();
    SD.end();
    return false;
  }

  const size_t written = Update.writeStream(firmware);
  firmware.close();

  Serial.printf("OTA: written %u / %u bytes\n",
                static_cast<unsigned>(written), static_cast<unsigned>(size));

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

  // Remove both files only after a successful OTA. This prevents repeated
  // updates while allowing a failed update to be retried on the next boot.
  const bool firmwareRemoved = SD.remove(firmwarePath);
  const bool versionRemoved = SD.remove(versionPath);

  Serial.printf("SD: PSXCore.bin removed: %s\n", firmwareRemoved ? "YES" : "NO");
  Serial.printf("SD: version.txt removed: %s\n", versionRemoved ? "YES" : "NO");

  SD.end();

  Serial.println("OTA: update complete");
  Serial.println("BOOT: restarting into new firmware...");
  delay(1000);
  ESP.restart();
  return true;
#endif
}
