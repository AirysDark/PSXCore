#include <Arduino.h>
#include <BleGamepad.h>
#include "controller_state.h"
#include "debug_status.h"
#include "ble_gamepad.h"
#include "psx_analog_mode.h"

BleGamepad bleGamepad("PSXCore ESP32-S3", "AirysDark", 100, true);

static bool lastConnected = false;
static bool configReady = false;
static uint32_t lastStateButtons = UINT32_MAX;
static uint8_t lastStateLx = 0xFF;
static uint8_t lastStateLy = 0xFF;
static uint8_t lastStateRx = 0xFF;
static uint8_t lastStateRy = 0xFF;

static int16_t psxAxisToHid(uint8_t value) {
  return static_cast<int16_t>((static_cast<uint32_t>(value) * 32767U + 127U) / 255U);
}

static void sendConfigHello() {
  bleConfigSendText("{\"type\":\"hello\",\"device\":\"PSXCore\",\"protocol\":1,\"transport\":\"ble-nus\",\"hid\":true,\"liveState\":true,\"ota\":\"pending\"}\n");
}

static void sendInfo() {
  bleConfigSendText("{\"type\":\"info\",\"device\":\"PSXCore\",\"protocol\":1,\"hid\":true,\"config\":true,\"liveState\":true,\"ota\":\"pending\"}\n");
}

static void sendSettings() {
  bleConfigSendText("{\"type\":\"settings\",\"analogControl\":true,\"sleepMinutes\":5}\n");
}

static void onConfigData(const uint8_t* data, size_t length) {
  if (!data || !length) return;

  String command;
  command.reserve(length);
  for (size_t i = 0; i < length; ++i) command += static_cast<char>(data[i]);
  command.trim();

  Serial.printf("[BLE-CFG] RX: %s\n", command.c_str());

  if (command == "PING") {
    bleConfigSendText("PONG\n");
  } else if (command == "HELLO" || command == "INFO") {
    sendInfo();
  } else if (command == "GET_STATE") {
    bleConfigNotifyControllerState();
  } else if (command == "GET_SETTINGS") {
    sendSettings();
  } else if (command == "SET_ANALOG") {
    psxEnableAnalogMode();
    bleConfigSendText("{\"type\":\"result\",\"command\":\"SET_ANALOG\",\"ok\":true}\n");
  } else if (command == "OTA_INFO") {
    bleConfigSendText("{\"type\":\"ota\",\"supported\":false,\"state\":\"pending\"}\n");
  } else {
    bleConfigSendText("{\"type\":\"error\",\"error\":\"unknown_command\"}\n");
  }
}

static void updatePsxButtons(uint32_t buttons) {
  static const struct { uint8_t psxBit; uint8_t hidButton; } mapping[] = {
      {15, BUTTON_1}, {14, BUTTON_2}, {13, BUTTON_3}, {12, BUTTON_4},
      {10, BUTTON_5}, {11, BUTTON_6}, {8, BUTTON_7},  {9, BUTTON_8},
      {1, BUTTON_9},  {2, BUTTON_10}, {0, BUTTON_11}, {3, BUTTON_12},
  };

  for (const auto &entry : mapping) {
    if (buttons & (1UL << entry.psxBit)) bleGamepad.press(entry.hidButton);
    else bleGamepad.release(entry.hidButton);
  }

  const bool up = buttons & (1UL << 4);
  const bool right = buttons & (1UL << 5);
  const bool down = buttons & (1UL << 6);
  const bool left = buttons & (1UL << 7);

  if (up && right) bleGamepad.setHat1(HAT_UP_RIGHT);
  else if (right && down) bleGamepad.setHat1(HAT_DOWN_RIGHT);
  else if (down && left) bleGamepad.setHat1(HAT_DOWN_LEFT);
  else if (left && up) bleGamepad.setHat1(HAT_UP_LEFT);
  else if (up) bleGamepad.setHat1(HAT_UP);
  else if (right) bleGamepad.setHat1(HAT_RIGHT);
  else if (down) bleGamepad.setHat1(HAT_DOWN);
  else if (left) bleGamepad.setHat1(HAT_LEFT);
  else bleGamepad.setHat1(HAT_CENTERED);
}

void ble_init() {
  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setButtonCount(16);
  config.setHatSwitchCount(1);

  bleGamepad.begin(&config);
  bleGamepad.beginNUS();
  bleGamepad.setNUSDataReceivedCallback(onConfigData);
  configReady = (bleGamepad.getNUS() != nullptr);

  Serial.println("[BLE] HID service: READY");
  Serial.printf("[BLE] Android config service: %s\n", configReady ? "READY" : "FAILED");
}

void ble_send_report() {
  const bool connected = bleGamepad.isConnected();
  debugStatusBLEState(connected);

  if (connected != lastConnected) {
    Serial.printf("[BLE] HID %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    lastConnected = connected;
    if (connected && configReady) sendConfigHello();
  }

  if (!connected) return;

  updatePsxButtons(controllerState.buttons);
  bleGamepad.setLeftThumb(psxAxisToHid(controllerState.lx), psxAxisToHid(controllerState.ly));
  bleGamepad.setRightThumb(psxAxisToHid(controllerState.rx), psxAxisToHid(controllerState.ry));
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

bool bleConfigIsReady() {
  return configReady;
}

void bleConfigSend(const uint8_t* data, size_t length) {
  if (!configReady || !data || !length) return;
  bleGamepad.sendDataOverNUS(data, length);
}

void bleConfigSendText(const char* text) {
  if (!text) return;
  bleConfigSend(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

void bleConfigNotifyControllerState() {
  if (!configReady) return;

  const ControllerState& state = controllerState;
  const bool changed = state.buttons != lastStateButtons ||
                       state.lx != lastStateLx || state.ly != lastStateLy ||
                       state.rx != lastStateRx || state.ry != lastStateRy;
  if (!changed) return;

  char message[160];
  snprintf(message, sizeof(message),
           "{\"type\":\"state\",\"buttons\":%lu,\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u}\n",
           static_cast<unsigned long>(state.buttons), state.lx, state.ly, state.rx, state.ry);
  bleConfigSendText(message);

  lastStateButtons = state.buttons;
  lastStateLx = state.lx;
  lastStateLy = state.ly;
  lastStateRx = state.rx;
  lastStateRy = state.ry;
}
