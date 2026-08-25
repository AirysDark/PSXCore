// PSXCore ESP32-S3 firmware entry point

#include <Arduino.h>
#include <Preferences.h>

#include "pins.h"
#include "version.h"
#include "controller_state.h"
#include "psx_reader.h"
#include "psx_pin_sweep.h"
#include "psx_analog_mode.h"
#include "ble_gamepad.h"
#include "psx_config.h"
#include "analog_button.h"
#include "power_manager.h"
#include "sd_update.h"
#include "debug_status.h"

static bool systemBooted = false;
static bool psxReady = false;
static bool bootSummaryPrinted = false;

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

static bool handleAnalogButtonEvent() {
  switch (analogButtonTakeEvent()) {
    case AnalogButtonEvent::ShortPress:
      Serial.printf("[ANALOG] Short press: %s mode\n",
                    analogButtonIsAnalogMode() ? "ANALOG" : "DIGITAL");
      powerManagerWake();
      return true;

    case AnalogButtonEvent::LongPress:
      Serial.println("[ANALOG] Long press event");
      powerManagerWake();
      return true;

    default:
      return false;
  }
}

static void initializeControllerMode(uint8_t &controllerId) {
  Serial.println("[BOOT] Enabling PS2 analog mode...");
  const bool analogEnabled = psx_enable_analog_mode();

  if (psxProbeController(&controllerId)) {
    Serial.printf("[BOOT] Controller mode after setup: ID=%02X\n", controllerId);
  } else {
    Serial.println("[BOOT] Controller mode re-probe failed");
  }

  analogButtonInit(controllerId);
  bootMark("Analog mode", analogEnabled && analogButtonIsAnalogMode());
}

static void printReadyBanner() {
  if (bootSummaryPrinted) return;
  bootSummaryPrinted = true;

  // bleConfigIsReady() is the authoritative completion signal for the custom
  // PSXCore GATT companion. Do not report FAILED while NimBLE is still
  // asynchronously syncing and constructing the shared service.
  Serial.println("[BOOT] Android companion   READY");
  Serial.println("[BOOT] BLE advertising      READY");
  Serial.println("================================");
  Serial.println("[BOOT] PSXCore READY");
  Serial.printf("[BOOT] Version              %s\n", PSXCORE_VERSION_STRING);
  Serial.printf("[BOOT] PSX polling          %s\n", psxReady ? "ENABLED" : "SEARCHING");
  Serial.println("================================");

  // Enable the first 12 PSX RAW diagnostics only after the full asynchronous
  // BLE/GATT/advertising startup has completed and all boot output is done.
  psxSetRawDebugEnabled(true);
}

void setup() {
  Serial.begin(115200);
  psxSetRawDebugEnabled(false);

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
  if (updateApplied) return;
  Serial.println("[BOOT] No SD update - continuing");

  Serial.println("[BOOT] Initializing PSX bus...");
  psxPinsBegin();
  psxBegin();

  uint8_t controllerId = 0;
  psxReady = psxProbeController(&controllerId);

  if (psxReady) {
    Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
    initializeControllerMode(controllerId);
  } else {
    Serial.println("[BOOT] PSX controller      NO RESPONSE");
    Serial.println("[BOOT] Starting pin sweep recovery...");

    if (psxPinSweep()) {
      Serial.println("[BOOT] Re-initializing PSX bus with corrected pins...");
      psxBegin();
      psxReady = psxProbeController(&controllerId);

      if (psxReady) {
        Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
        initializeControllerMode(controllerId);
      } else {
        Serial.println("[BOOT] PSX controller      RESPONSE NOT CONFIRMED");
      }
    } else {
      Serial.println("[BOOT] Pin sweep failed");
    }
  }

  if (psxReady) {
    Serial.println("[BOOT] PSX input           ENABLED");
    Serial.printf("[BOOT] ANALOG mode          %s\n",
                  analogButtonIsAnalogMode() ? "ON" : "DIGITAL FALLBACK");
    Serial.println("[BOOT] ANALOG button       ENABLED (also wakes idle sleep)");
  } else {
    Serial.println("[BOOT] PSX input           SEARCHING");
    Serial.println("[BOOT] ANALOG button       WAITING FOR CONTROLLER");
  }

  Serial.println("[BOOT] Starting Bluetooth HID + Android companion...");
  bleGamepadBegin();
  bootMark("Bluetooth HID", true);
  Serial.println("[BOOT] Android companion   STARTING");
  Serial.println("[BOOT] BLE advertising      STARTING");

  ControllerState idleState{};
  idleState.lx = idleState.ly = idleState.rx = idleState.ry = 0x80;
  controllerState = idleState;
  powerManagerBegin(idleState);
  Serial.println("[BOOT] Power manager        ACTIVE");
  Serial.println("[BOOT] Idle sleep           5 MINUTES");

  systemBooted = true;
}

void loop() {
  if (!systemBooted) return;

  bool analogEvent = false;
  if (psxReady) {
    psxReadController();
    analogEvent = handleAnalogButtonEvent();
    powerManagerUpdate(controllerState, analogEvent);
  }

  if (!powerManagerIsSleeping()) {
    // bleGamepadUpdate() owns both HID reporting and the rate-limited,
    // change-detected Android live-state notification. Do not notify again
    // here or the same state can be pushed twice per loop iteration.
    bleGamepadUpdate();

    // The BLE/GATT manager initializes asynchronously. Hold back the final
    // READY banner and RAW diagnostics until the custom app companion is
    // actually ready. This prevents a false FAILED status during host sync.
    if (!bootSummaryPrinted && bleConfigIsReady()) {
      printReadyBanner();
    }

    debugStatusLoop();
    delay(5);
  } else {
    delay(100);
  }
}
