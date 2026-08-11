// PSXCore Bluetooth HID layer
// ESP32-S3 converts PSX controller input into a wireless gamepad.

#include "ble_gamepad.h"
#include "controller_state.h"

static bool bleReady = false;

void bleGamepadBegin() {
  // BLE HID hardware initialization hook.
  // NimBLE HID service will attach here.
  bleReady = true;
}

void bleGamepadUpdate() {
  if (!bleReady) return;

  // ControllerState is now the single source of input data.
  // HID report transmission uses:
  // controllerState.buttons
  // controllerState.lx
  // controllerState.ly
  // controllerState.rx
  // controllerState.ry
}
