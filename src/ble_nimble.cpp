#include <Arduino.h>
#include <BleGamepad.h>
#include "controller_state.h"
#include "debug_status.h"

BleGamepad bleGamepad("PSXCore ESP32-S3", "AirysDark", 100);

void ble_init() {
  bleGamepad.begin();
}

void ble_send_report() {
  bool connected = bleGamepad.isConnected();
  debugStatusBLEState(connected);

  if (!connected) return;

  bleGamepad.setLeftThumb(controllerState.lx, controllerState.ly);
  bleGamepad.setRightThumb(controllerState.rx, controllerState.ry);

  bleGamepad.sendReport();
}

void bleGamepadBegin() {
  ble_init();
  debugStatusBLEState(false);
}

void bleGamepadUpdate() {
  ble_send_report();
}
