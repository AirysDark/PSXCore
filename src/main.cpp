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
  if (updateApplied) return;
  Serial.println("[BOOT] No SD update - continuing");

  Serial.println("[BOOT] Initializing PSX bus...");
  psxPinsBegin();
  psxBegin();

  uint8_t controllerId = 0;
  psxReady = psxProbeController(&controllerId);

  if (psxReady) {
    Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
    analogButtonInit(controllerId);
  } else {
    Serial.println("[BOOT] PSX controller      NO RESPONSE");
    Serial.println("[BOOT] Starting pin sweep recovery...");

    if (psxPinSweep()) {
      Serial.println("[BOOT] Re-initializing PSX bus with corrected pins...");
      psxBegin();
      psxReady = psxProbeController(&controllerId);

      if (psxReady) {
        Serial.printf("[BOOT] PSX controller      OK (ID=%02X)\n", controllerId);
        analogButtonInit(controllerId);
      } else {
        Serial.println("[BOOT] PSX controller      RESPONSE NOT CONFIRMED");
      }
    } else {
      Serial.println("[BOOT] Pin sweep failed");
    }
  }

  if (psxReady) {
    Serial.println("[BOOT] PSX input           ENABLED");
    Serial.println("[BOOT] ANALOG button       ENABLED (also wakes idle sleep)");
  } else {
    Serial.println("[BOOT] PSX input           SEARCHING");
    Serial.println("[BOOT] ANALOG button       WAITING FOR CONTROLLER");
  }

  Serial.println("[BOOT] Starting Bluetooth HID + Android companion...");
  bleGamepadBegin();
  bootMark("Bluetooth HID", true);
  Serial.printf("[BOOT] Android companion   %s\n", bleConfigIsReady() ? "READY" : "FAILED");
  Serial.println("[BOOT] BLE advertising      ON");

  ControllerState idleState{};
  idleState.lx = idleState.ly = idleState.rx = idleState.ry = 0x80;
  controllerState = idleState;
  powerManagerBegin(idleState);
  Serial.println("[BOOT] Power manager        ACTIVE");
  Serial.println("[BOOT] Idle sleep           5 MINUTES");

  systemBooted = true;

  Serial.println("================================");
  Serial.println("[BOOT] PSXCore READY");
  Serial.printf("[BOOT] Version              %s\n", PSXCORE_VERSION_STRING);
  Serial.printf("[BOOT] PSX polling          %s\n", psxReady ? "ENABLED" : "SEARCHING");
  Serial.println("================================");
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
    bleGamepadUpdate();
    bleConfigNotifyControllerState();
    debugStatusLoop();
    delay(5);
  } else {
    delay(100);
  }
}
