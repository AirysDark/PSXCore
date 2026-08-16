// PSXCore ESP32-S3 firmware entry point
// Safe boot diagnostics: each subsystem is isolated so an early reset can be located.

#include <Arduino.h>
#include <Preferences.h>

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
static bool psxReady = false;

static void bootMark(const char *name, bool ok) {
  Serial.print("[BOOT] ");
  Serial.print(name);
  Serial.print(" ");
  Serial.println(ok ? "OK" : "FAIL");
}

static bool testNvs() {
  Preferences prefs;
  if (!prefs.begin("psxcore", false)) {
    return false;
  }
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("          PSXCore BOOT");
  Serial.println("================================");

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

  Serial.println("[BOOT] Checking SD update...");
  bool sdOk = false;
  // SD failure must never prevent the controller/BLE boot path.
  sdOk = sdUpdateCheck();
  bootMark("SD updater", sdOk);

  Serial.println("[BOOT] Initializing PSX bus...");
  psxBegin();

  uint8_t controllerId = 0;
  psxReady = psxProbeController(&controllerId);

  if (psxReady) {
    Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
  } else {
    Serial.println("[BOOT] PSX controller      NO RESPONSE");
    Serial.println("[BOOT] Starting pin sweep recovery...");

    if (psxPinSweep()) {
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

  bootMark("PSX analog config", psxEnableAnalogMode());

  Serial.println("[BOOT] Starting Bluetooth HID...");
  bleGamepadBegin();
  bootMark("Bluetooth HID", true);
  Serial.println("[BOOT] BLE advertising      ON");

  systemBooted = true;

  Serial.println("================================");
  Serial.println("[BOOT] PSXCore READY");
  Serial.printf("[BOOT] PSX polling          %s\n", psxReady ? "AVAILABLE" : "DISABLED");
  Serial.println("================================");
}

void loop() {
  if (!systemBooted) return;

  // PSX polling remains disabled until the boot probe has confirmed a controller.
  // This prevents an unconnected bus from flooding the serial console.
  if (psxReady) {
    psxReadController();
  }

  bleGamepadUpdate();
  debugStatusLoop();
  delay(5);
}
