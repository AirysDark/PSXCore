// PSXCore ESP32-S3 firmware entry point

#include <Arduino.h>
#include <Preferences.h>

#include "pins.h"
#include "version.h"
#include "sd_update.h"
#include "debug_status.h"

#include <PSXController.h>
#include <PSXInputMapper.h>
#include <PSXBLEGamepad.h>

static bool systemBooted = false;
static bool psxReady = false;

static PSXController psxController;
static PSXInputMapper inputMapper;
static PSXBLEGamepad bleGamepad;

static void bootMark(const char *name, bool ok) {
  Serial.print("[BOOT] ");
  Serial.print(name);
  Serial.print(" ");
  Serial.println(ok ? "OK" : "FAIL");
}

static bool testNvs() {
  Preferences prefs;
  if (!prefs.begin("psxcore", false)) return false;
  prefs.putUInt("boot", millis());
  prefs.end();
  return true;
}

static bool testPsram() {
#if CONFIG_SPIRAM_SUPPORT
  if (!psramFound()) return false;
  void *p = ps_malloc(256);
  if (!p) return false;
  memset(p, 0xA5, 256);
  bool ok = true;
  for (size_t i = 0; i < 256; ++i) {
    if (static_cast<uint8_t *>(p)[i] != 0xA5) {
      ok = false;
      break;
    }
  }
  free(p);
  return ok;
#else
  return true;
#endif
}

static bool beginPSXController() {
  PSXControllerPins pins;
  pins.data = PSX_DATA;
  pins.command = PSX_COMMAND;
  pins.attention = PSX_ATTENTION;
  pins.clock = PSX_CLOCK;

  PSXControllerConfig config;
  config.requestAnalog = true;
  config.requestPressure = true;
  config.lockAnalogMode = true;

  return psxController.begin(pins, config);
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println("          PSXCore BOOT");
  Serial.println("================================");
  Serial.printf("[BOOT] Version          %s\n", PSXCORE_VERSION_STRING);

  debugStatusInit();
  bootMark("Debug system", true);

  Serial.println("[BOOT] Memory/runtime diagnostics...");
  Serial.printf("[BOOT] Free heap        %u\n", ESP.getFreeHeap());
#if CONFIG_SPIRAM_SUPPORT
  Serial.printf("[BOOT] PSRAM detected   %s\n", psramFound() ? "YES" : "NO");
  Serial.printf("[BOOT] PSRAM size       %u\n", ESP.getPsramSize());
  Serial.printf("[BOOT] PSRAM free       %u\n", ESP.getFreePsram());
#endif
  bootMark("PSRAM test", testPsram());
  bootMark("NVS test", testNvs());

  Serial.println("[BOOT] Checking optional SD update...");
  const bool updateApplied = sdUpdateCheck();
  if (updateApplied) {
    return;
  }
  Serial.println("[BOOT] No SD update - continuing");

  Serial.println("[BOOT] Initializing PSXController library...");
  psxReady = beginPSXController();
  if (psxReady) {
    const PSXControllerState& state = psxController.state();
    Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", state.mode);
    Serial.printf("[BOOT] PSX mode            %s\n",
                  state.pressureMode ? "PRESSURE" : (state.analogMode ? "ANALOG" : "DIGITAL"));
    Serial.println("[BOOT] PSX input           ENABLED");
  } else {
    Serial.println("[BOOT] PSX controller      NO RESPONSE");
    Serial.println("[BOOT] PSX input           SEARCHING");
  }

  Serial.println("[BOOT] Starting custom Bluetooth HID...");
  const bool bleReady = bleGamepad.begin("PSXCore Controller");
  bootMark("Bluetooth HID", bleReady);
  Serial.println(bleReady ? "[BOOT] BLE advertising      ON" : "[BOOT] BLE advertising      FAIL");

  systemBooted = bleReady;

  Serial.println("================================");
  Serial.println(systemBooted ? "[BOOT] PSXCore READY" : "[BOOT] PSXCore STARTUP FAILED");
  Serial.printf("[BOOT] Version              %s\n", PSXCORE_VERSION_STRING);
  Serial.printf("[BOOT] PSX polling          %s\n", psxReady ? "ENABLED" : "SEARCHING");
  Serial.println("================================");
}

void loop() {
  if (!systemBooted) return;

  psxReady = psxController.update();

  const PSXInputState input = inputMapper.map(psxController.state());
  bleGamepad.send(input);

  debugStatusLoop();
  delay(5);
}
