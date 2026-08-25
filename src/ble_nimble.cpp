#include <Arduino.h>
#include <BleGamepad.h>
#include <Update.h>
#include <esp_partition.h>
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

enum class OtaState : uint8_t { Idle, Ready, Receiving, Success, Error };
static OtaState otaState = OtaState::Idle;
static size_t otaExpectedSize = 0;
static size_t otaReceivedSize = 0;
static uint8_t otaProgress = 0;
static esp_err_t otaLastError = ESP_OK;

static const char* otaStateName(OtaState state) {
  switch (state) {
    case OtaState::Idle: return "idle";
    case OtaState::Ready: return "ready";
    case OtaState::Receiving: return "receiving";
    case OtaState::Success: return "success";
    case OtaState::Error: return "error";
    default: return "unknown";
  }
}

static size_t otaAvailableSpace() {
  const esp_partition_t* p = esp_ota_get_next_update_partition(nullptr);
  return p ? p->size : 0;
}

static void resetOtaState() {
  otaState = OtaState::Idle;
  otaExpectedSize = 0;
  otaReceivedSize = 0;
  otaProgress = 0;
  otaLastError = ESP_OK;
}

static void abortOta(const char* reason) {
  if (Update.isRunning()) Update.abort();
  otaState = OtaState::Error;
  otaLastError = static_cast<esp_err_t>(Update.getError());
  Serial.printf("[OTA] ERROR: %s (Update error=%u)\n", reason ? reason : "unknown", Update.getError());
  char msg[192];
  snprintf(msg, sizeof(msg), "{\"type\":\"ota\",\"state\":\"error\",\"reason\":\"%s\",\"error\":%u}\n", reason ? reason : "unknown", Update.getError());
  bleConfigSendText(msg);
}

static void sendOtaInfo() {
  char message[256];
  snprintf(message, sizeof(message),
    "{\"type\":\"ota\",\"supported\":true,\"state\":\"%s\",\"expectedSize\":%lu,\"receivedSize\":%lu,\"progress\":%u,\"availableSpace\":%lu,\"updateInProgress\":%s,\"error\":%d}\n",
    otaStateName(otaState), (unsigned long)otaExpectedSize, (unsigned long)otaReceivedSize,
    otaProgress, (unsigned long)otaAvailableSpace(), Update.isRunning() ? "true" : "false", (int)otaLastError);
  bleConfigSendText(message);
}

// PART 2: Open and validate the inactive OTA partition.
static bool beginOta(size_t firmwareSize) {
  if (firmwareSize == 0) {
    abortOta("invalid_size");
    return false;
  }
  if (Update.isRunning()) {
    abortOta("update_already_running");
    return false;
  }
  const size_t available = otaAvailableSpace();
  if (available == 0) {
    abortOta("no_ota_partition");
    return false;
  }
  if (firmwareSize > available) {
    otaExpectedSize = firmwareSize;
    abortOta("firmware_too_large");
    return false;
  }

  resetOtaState();
  otaExpectedSize = firmwareSize;
  otaState = OtaState::Ready;

  Serial.printf("[OTA] BEGIN requested: %lu bytes, slot=%lu bytes\n", (unsigned long)firmwareSize, (unsigned long)available);

  if (!Update.begin(firmwareSize, U_FLASH)) {
    abortOta("begin_failed");
    return false;
  }

  otaState = OtaState::Receiving;
  Serial.println("[OTA] UPDATE PARTITION OPEN - READY FOR FIRMWARE DATA");
  char msg[192];
  snprintf(msg, sizeof(msg), "{\"type\":\"ota\",\"state\":\"receiving\",\"ok\":true,\"expectedSize\":%lu,\"receivedSize\":0,\"progress\":0}\n", (unsigned long)otaExpectedSize);
  bleConfigSendText(msg);
  return true;
}

static int16_t psxAxisToHid(uint8_t value) { return (int16_t)((uint32_t(value) * 32767U + 127U) / 255U); }
static void sendConfigHello() { bleConfigSendText("{\"type\":\"hello\",\"device\":\"PSXCore\",\"protocol\":2,\"transport\":\"ble-nus\",\"hid\":true,\"liveState\":true,\"ota\":\"ready\",\"otaSupported\":true}\n"); }
static void sendInfo() { bleConfigSendText("{\"type\":\"info\",\"device\":\"PSXCore\",\"protocol\":2,\"hid\":true,\"config\":true,\"liveState\":true,\"ota\":\"ready\",\"otaSupported\":true}\n"); }
static void sendSettings() { bleConfigSendText("{\"type\":\"settings\",\"analogControl\":true,\"sleepMinutes\":5}\n"); }

static void onConfigData(const uint8_t* data, size_t length) {
  if (!data || !length) return;
  String command;
  command.reserve(length);
  for (size_t i = 0; i < length; ++i) command += (char)data[i];
  command.trim();
  Serial.printf("[BLE-CFG] RX: %s\n", command.c_str());

  if (command == "PING") bleConfigSendText("PONG\n");
  else if (command == "HELLO" || command == "INFO") sendInfo();
  else if (command == "GET_STATE") bleConfigNotifyControllerState();
  else if (command == "GET_SETTINGS") sendSettings();
  else if (command == "SET_ANALOG") {
    psxEnableAnalogMode();
    bleConfigSendText("{\"type\":\"result\",\"command\":\"SET_ANALOG\",\"ok\":true}\n");
  } else if (command == "OTA_INFO") sendOtaInfo();
  else if (command.startsWith("OTA_BEGIN:")) {
    String sizeText = command.substring(strlen("OTA_BEGIN:"));
    sizeText.trim();
    char* end = nullptr;
    unsigned long sizeValue = strtoul(sizeText.c_str(), &end, 10);
    if (!sizeText.length() || end == sizeText.c_str() || *end != '\0') abortOta("invalid_begin_command");
    else beginOta((size_t)sizeValue);
  } else if (command == "OTA_RESET") {
    if (Update.isRunning()) Update.abort();
    resetOtaState();
    sendOtaInfo();
  } else bleConfigSendText("{\"type\":\"error\",\"error\":\"unknown_command\"}\n");
}

static void updatePsxButtons(uint32_t buttons) {
  static const struct { uint8_t psxBit; uint8_t hidButton; } mapping[] = {
    {15,BUTTON_1},{14,BUTTON_2},{13,BUTTON_3},{12,BUTTON_4},{10,BUTTON_5},{11,BUTTON_6},{8,BUTTON_7},{9,BUTTON_8},{1,BUTTON_9},{2,BUTTON_10},{0,BUTTON_11},{3,BUTTON_12}
  };
  for (const auto& e : mapping) { if (buttons & (1UL << e.psxBit)) bleGamepad.press(e.hidButton); else bleGamepad.release(e.hidButton); }
  bool up=buttons&(1UL<<4), right=buttons&(1UL<<5), down=buttons&(1UL<<6), left=buttons&(1UL<<7);
  if(up&&right)bleGamepad.setHat1(HAT_UP_RIGHT); else if(right&&down)bleGamepad.setHat1(HAT_DOWN_RIGHT); else if(down&&left)bleGamepad.setHat1(HAT_DOWN_LEFT); else if(left&&up)bleGamepad.setHat1(HAT_UP_LEFT); else if(up)bleGamepad.setHat1(HAT_UP); else if(right)bleGamepad.setHat1(HAT_RIGHT); else if(down)bleGamepad.setHat1(HAT_DOWN); else if(left)bleGamepad.setHat1(HAT_LEFT); else bleGamepad.setHat1(HAT_CENTERED);
}

void ble_init() {
  BleGamepadConfiguration config;
  config.setAutoReport(false); config.setButtonCount(16); config.setHatSwitchCount(1);
  resetOtaState(); bleGamepad.begin(&config); bleGamepad.beginNUS(); bleGamepad.setNUSDataReceivedCallback(onConfigData);
  configReady = (bleGamepad.getNUS() != nullptr);
  Serial.println("[BLE] HID service: READY");
  Serial.printf("[BLE] Android config service: %s\n", configReady ? "READY" : "FAILED");
  Serial.printf("[OTA] Part 2 BEGIN support: %s, next slot=%lu bytes\n", configReady ? "READY" : "WAITING", (unsigned long)otaAvailableSpace());
}

void ble_send_report() {
  bool connected=bleGamepad.isConnected(); debugStatusBLEState(connected);
  if(connected!=lastConnected){Serial.printf("[BLE] HID %s\n",connected?"CONNECTED":"DISCONNECTED");lastConnected=connected;if(connected&&configReady)sendConfigHello();}
  if(!connected)return;
  updatePsxButtons(controllerState.buttons);
  bleGamepad.setLeftThumb(psxAxisToHid(controllerState.lx),psxAxisToHid(controllerState.ly));
  bleGamepad.setRightThumb(psxAxisToHid(controllerState.rx),psxAxisToHid(controllerState.ry));
  bleGamepad.sendReport(); debugStatusBLEUpdate();
}
void bleGamepadBegin(){ble_init();debugStatusBLEState(false);} void bleGamepadUpdate(){ble_send_report();}
bool bleConfigIsReady(){return configReady;}
void bleConfigSend(const uint8_t* data,size_t length){if(configReady&&data&&length)bleGamepad.sendDataOverNUS(data,length);}
void bleConfigSendText(const char* text){if(text)bleConfigSend(reinterpret_cast<const uint8_t*>(text),strlen(text));}
void bleConfigNotifyControllerState(){
  if(!configReady)return; const ControllerState& s=controllerState;
  bool changed=s.buttons!=lastStateButtons||s.lx!=lastStateLx||s.ly!=lastStateLy||s.rx!=lastStateRx||s.ry!=lastStateRy; if(!changed)return;
  char m[160];snprintf(m,sizeof(m),"{\"type\":\"state\",\"buttons\":%lu,\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u}\n",(unsigned long)s.buttons,s.lx,s.ly,s.rx,s.ry);bleConfigSendText(m);
  lastStateButtons=s.buttons;lastStateLx=s.lx;lastStateLy=s.ly;lastStateRx=s.rx;lastStateRy=s.ry;
}
