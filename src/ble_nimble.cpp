#include <Arduino.h>
#include <BleGamepad.h>

#include "controller_state.h"
#include "debug_status.h"
#include "ble_gamepad.h"

BleGamepad bleGamepad("PSXCore", "AirysDark", 100, true);

static bool lastConnected = false;

static int16_t psxAxisToHid(uint8_t value) {
  return static_cast<int16_t>((static_cast<uint32_t>(value) * 32767U + 127U) / 255U);
}

static void updatePsxButtons(uint32_t buttons) {
  static const struct {
    uint8_t psxBit;
    uint8_t hidButton;
  } map[] = {
    {15, BUTTON_1},
    {14, BUTTON_2},
    {13, BUTTON_3},
    {12, BUTTON_4},
    {10, BUTTON_5},
    {11, BUTTON_6},
    {8,  BUTTON_7},
    {9,  BUTTON_8},
    {1,  BUTTON_9},
    {2,  BUTTON_10},
    {0,  BUTTON_11},
    {3,  BUTTON_12},
  };

  for (const auto &entry : map) {
    if (buttons & (1UL << entry.psxBit)) {
      bleGamepad.press(entry.hidButton);
    } else {
      bleGamepad.release(entry.hidButton);
    }
  }

  const bool up    = buttons & (1UL << 4);
  const bool right = buttons & (1UL << 5);
  const bool down  = buttons & (1UL << 6);
  const bool left  = buttons & (1UL << 7);

  if (up && right) {
    bleGamepad.setHat1(HAT_UP_RIGHT);
  } else if (right && down) {
    bleGamepad.setHat1(HAT_DOWN_RIGHT);
  } else if (down && left) {
    bleGamepad.setHat1(HAT_DOWN_LEFT);
  } else if (left && up) {
    bleGamepad.setHat1(HAT_UP_LEFT);
  } else if (up) {
    bleGamepad.setHat1(HAT_UP);
  } else if (right) {
    bleGamepad.setHat1(HAT_RIGHT);
  } else if (down) {
    bleGamepad.setHat1(HAT_DOWN);
  } else if (left) {
    bleGamepad.setHat1(HAT_LEFT);
  } else {
    bleGamepad.setHat1(HAT_CENTERED);
  }
}

void bleGamepadBegin() {
  BleGamepadConfiguration configuration;
  configuration.setAutoReport(false);
  configuration.setButtonCount(16);
  configuration.setHatSwitchCount(1);

  bleGamepad.begin(&configuration);
  lastConnected = false;
  debugStatusBLEState(false);

  Serial.println("[BLE] Bluetooth HID gamepad started");
  Serial.println("[BLE] Android companion/custom GATT: REMOVED");
}

void bleGamepadUpdate() {
  const bool connected = bleGamepad.isConnected();
  debugStatusBLEState(connected);

  if (connected != lastConnected) {
    Serial.printf("[BLE] HID %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    lastConnected = connected;
  }

  if (!connected) {
    return;
  }

  updatePsxButtons(controllerState.buttons);
  bleGamepad.setLeftThumb(
      psxAxisToHid(controllerState.lx),
      psxAxisToHid(controllerState.ly));
  bleGamepad.setRightThumb(
      psxAxisToHid(controllerState.rx),
      psxAxisToHid(controllerState.ry));
  bleGamepad.sendReport();
  debugStatusBLEUpdate();
}
