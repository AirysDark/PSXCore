#include <Arduino.h>
#include "power_manager.h"

static ControllerState lastState{};
static uint32_t lastActivityMs = 0;
static bool sleeping = false;

static bool stateChanged(const ControllerState& a, const ControllerState& b) {
  return a.buttons != b.buttons ||
         a.lx != b.lx || a.ly != b.ly ||
         a.rx != b.rx || a.ry != b.ry;
}

static bool hasInput(const ControllerState& state) {
  constexpr uint8_t center = 0x80;
  constexpr uint8_t deadzone = 8;

  return state.buttons != 0 ||
         abs((int)state.lx - center) > deadzone ||
         abs((int)state.ly - center) > deadzone ||
         abs((int)state.rx - center) > deadzone ||
         abs((int)state.ry - center) > deadzone;
}

void powerManagerBegin(const ControllerState& initialState) {
  lastState = initialState;
  lastActivityMs = millis();
  sleeping = false;
}

void powerManagerWake() {
  if (sleeping) {
    sleeping = false;
    Serial.println("[POWER] Wake: controller activity detected");
  }
  lastActivityMs = millis();
}

void powerManagerUpdate(const ControllerState& state, bool analogEvent) {
  const uint32_t now = millis();
  const bool activity = analogEvent || hasInput(state) || stateChanged(state, lastState);

  if (activity) {
    powerManagerWake();
  } else if (!sleeping && (uint32_t)(now - lastActivityMs) >= PSXCORE_IDLE_SLEEP_MS) {
    sleeping = true;
    Serial.println("[POWER] Sleep: 5 minutes with no controller input");
  }

  lastState = state;
}

bool powerManagerIsSleeping() {
  return sleeping;
}
