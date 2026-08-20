#include <Arduino.h>
#include <BleGamepad.h>
#include "controller_state.h"
#include "debug_status.h"

BleGamepad bleGamepad("PSXCore ESP32-S3", "AirysDark", 100);

static bool lastConnected = false;
static uint32_t lastButtonState = 0;

static int16_t psxAxisToHid(uint8_t value) {
  // PS2 axes are unsigned 8-bit (0..255), while the default
  // ESP32-BLE-Gamepad HID axis range is 0..32767.
  return static_cast<int16_t>((static_cast<uint32_t>(value) * 32767U + 127U) / 255U);
}

static void setButton(uint8_t button, bool pressed, uint32_t mask) {
  if (pressed) {
    bleGamepad.press(button);
  } else {
    bleGamepad.release(button);
  }
}

static void updatePsxButtons(uint32_t buttons) {
  // PS2 button bits after the active-low -> active-high conversion:
  // Low byte:  SELECT, L3, R3, START, UP, RIGHT, DOWN, LEFT
  // High byte: L2, R2, L1, R1, TRIANGLE, CIRCLE, CROSS, SQUARE
  //
  // BLE mapping deliberately follows a stable logical order:
  // 1 Square, 2 Cross, 3 Circle, 4 Triangle,
  // 5 L1, 6 R1, 7 L2, 8 R2, 9 L3, 10 R3,
  // 11 Select, 12 Start.
  static const struct {
    uint8_t psxBit;
    uint8_t hidButton;
  } mapping[] = {
      {15, BUTTON_1},  // Square
      {14, BUTTON_2},  // Cross
      {13, BUTTON_3},  // Circle
      {12, BUTTON_4},  // Triangle
      {10, BUTTON_5},  // L1
      {11, BUTTON_6},  // R1
      {8,  BUTTON_7},  // L2
      {9,  BUTTON_8},  // R2
      {1,  BUTTON_9},  // L3
      {2,  BUTTON_10}, // R3
      {0,  BUTTON_11}, // Select
      {3,  BUTTON_12}, // Start
  };

  for (const auto &entry : mapping) {
    const bool pressed = (buttons & (1UL << entry.psxBit)) != 0;
    setButton(entry.hidButton, pressed, (1UL << entry.psxBit));
  }

  // D-pad is represented as HID hat 1 rather than four independent buttons.
  const bool up = (buttons & (1UL << 4)) != 0;
  const bool right = (buttons & (1UL << 5)) != 0;
  const bool down = (buttons & (1UL << 6)) != 0;
  const bool left = (buttons & (1UL << 7)) != 0;

  if (up && right) bleGamepad.setHat1(HAT_UP_RIGHT);
  else if (right && down) bleGamepad.setHat1(HAT_DOWN_RIGHT);
  else if (down && left) bleGamepad.setHat1(HAT_DOWN_LEFT);
  else if (left && up) bleGamepad.setHat1(HAT_UP_LEFT);
  else if (up) bleGamepad.setHat1(HAT_UP);
  else if (right) bleGamepad.setHat1(HAT_RIGHT);
  else if (down) bleGamepad.setHat1(HAT_DOWN);
  else if (left) bleGamepad.setHat1(HAT_LEFT);
  else bleGamepad.setHat1(HAT_CENTERED);

  lastButtonState = buttons;
}

void ble_init() {
  // Disable automatic reports. PSXCore controls the complete HID report so
  // buttons, d-pad and both sticks are updated atomically once per poll.
  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setButtonCount(16);
  config.setHatSwitchCount(1);
  bleGamepad.begin(&config);
}

void ble_send_report() {
  const bool connected = bleGamepad.isConnected();
  debugStatusBLEState(connected);

  if (connected != lastConnected) {
    Serial.printf("[BLE] %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    lastConnected = connected;
  }

  if (!connected) return;

  updatePsxButtons(controllerState.buttons);

  bleGamepad.setLeftThumb(psxAxisToHid(controllerState.lx),
                          psxAxisToHid(controllerState.ly));
  bleGamepad.setRightThumb(psxAxisToHid(controllerState.rx),
                           psxAxisToHid(controllerState.ry));

  bleGamepad.sendReport();
  debugStatusBLEUpdate();
}

void bleGamepadBegin() {
  ble_init();
  debugStatusBLEState(false);
}

void bleGamepadUpdate() {
  ble_send_report();
}
