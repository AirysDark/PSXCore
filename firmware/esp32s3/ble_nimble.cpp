#include <Arduino.h>
#include <BleGamepad.h>
#include "controller_state.h"

BleGamepad bleGamepad("PSXCore ESP32-S3", "AirysDark", 100);

void ble_init() {
  bleGamepad.begin();
}

void ble_send_report() {
  if (!bleGamepad.isConnected()) return;

  bleGamepad.setLeftThumb(controllerState.lx, controllerState.ly);
  bleGamepad.setRightThumb(controllerState.rx, controllerState.ry);

  bleGamepad.sendReport();
}

void bleGamepadBegin() {
  ble_init();
}

void bleGamepadUpdate() {
  ble_send_report();
}
