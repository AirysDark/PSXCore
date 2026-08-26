#include <Arduino.h>
#include <BleGamepad.h>
#include <NimBLEDevice.h>
#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "controller_state.h"
#include "debug_status.h"
#include "ble_gamepad.h"
#include "psx_analog_mode.h"
#include "psxcore_gatt.h"

BleGamepad bleGamepad("PSXCore", "AirysDark", 100, true);

static bool lastConnected = false;
static bool configReady = false;
static bool configInitPending = false;
static bool advertisingConfigured = false;
static uint32_t lastConfigInitAttempt = 0;
static uint32_t lastAdvertisingCheck = 0;
static uint32_t lastStateButtons = UINT32_MAX;
static uint8_t lastStateLx = 0xFF, lastStateLy = 0xFF, lastStateRx = 0xFF, lastStateRy = 0xFF;

static const char* BLE_DEVICE_NAME = "PSXCore";
static const char* HID_SERVICE_UUID = "1812";
static const char* PSXCORE_FIRMWARE_VERSION = "1.0.2";
static const char* PSXCORE_HARDWARE = "ESP32-S3";
static const char* PSXCORE_BUILD = __DATE__ " " __TIME__;

enum class OtaState : uint8_t { Idle, Ready, Receiving, Success, Error };
static OtaState otaState = OtaState::Idle;
static size_t otaExpectedSize = 0, otaReceivedSize = 0;
static uint8_t otaProgress = 0;
static esp_err_t otaLastError = ESP_OK;
static uint8_t otaLastReportedProgress = 255;
static uint32_t otaCompletedAt = 0;

static const char* otaStateName(OtaState s) { switch (s) { case OtaState::Idle:return "idle"; case OtaState::Ready:return "ready"; case OtaState::Receiving:return "receiving"; case OtaState::Success:return "success"; case OtaState::Error:return "error"; default:return "unknown"; } }
static size_t otaAvailableSpace() { const esp_partition_t* p=esp_ota_get_next_update_partition(nullptr); return p?p->size:0; }
static void resetOtaState() { otaState=OtaState::Idle; otaExpectedSize=0; otaReceivedSize=0; otaProgress=0; otaLastError=ESP_OK; otaLastReportedProgress=255; otaCompletedAt=0; }
static void sendOtaText(const char* text) { if (text) psxCoreGattSendOtaStatusText(text); }

static void abortOta(const char* reason) { if (Update.isRunning()) Update.abort(); otaState=OtaState::Error; otaLastError=static_cast<esp_err_t>(Update.getError()); Serial.printf("[OTA] ERROR: %s (Update error=%u)\n",reason?reason:"unknown",Update.getError()); char m[192]; snprintf(m,sizeof(m),"{\"type\":\"ota\",\"state\":\"error\",\"reason\":\"%s\",\"error\":%u}\n",reason?reason:"unknown",Update.getError()); sendOtaText(m); }
static void sendOtaInfo() { char m[256]; snprintf(m,sizeof(m),"{\"type\":\"ota\",\"supported\":true,\"state\":\"%s\",\"expectedSize\":%lu,\"receivedSize\":%lu,\"progress\":%u,\"availableSpace\":%lu,\"updateInProgress\":%s,\"error\":%d}\n",otaStateName(otaState),(unsigned long)otaExpectedSize,(unsigned long)otaReceivedSize,otaProgress,(unsigned long)otaAvailableSpace(),Update.isRunning()?"true":"false",(int)otaLastError); sendOtaText(m); }
static void sendOtaProgress(bool force=false) { if (!otaExpectedSize) return; uint8_t progress=(uint8_t)((otaReceivedSize*100ULL)/otaExpectedSize); if(progress>100)progress=100; otaProgress=progress; if(!force && progress==otaLastReportedProgress) return; otaLastReportedProgress=progress; char m[192]; snprintf(m,sizeof(m),"{\"type\":\"ota\",\"state\":\"receiving\",\"receivedSize\":%lu,\"expectedSize\":%lu,\"progress\":%u}\n",(unsigned long)otaReceivedSize,(unsigned long)otaExpectedSize,otaProgress); sendOtaText(m); }
static bool beginOta(size_t firmwareSize) { if(!firmwareSize){abortOta("invalid_size");return false;} if(Update.isRunning()){abortOta("update_already_running");return false;} size_t available=otaAvailableSpace(); if(!available){abortOta("no_ota_partition");return false;} if(firmwareSize>available){otaExpectedSize=firmwareSize;abortOta("firmware_too_large");return false;} resetOtaState(); otaExpectedSize=firmwareSize; otaState=OtaState::Ready; Serial.printf("[OTA] BEGIN requested: %lu bytes, slot=%lu bytes\n",(unsigned long)firmwareSize,(unsigned long)available); if(!Update.begin(firmwareSize,U_FLASH)){abortOta("begin_failed");return false;} otaState=OtaState::Receiving; Serial.println("[OTA] UPDATE PARTITION OPEN - READY FOR FIRMWARE DATA"); sendOtaProgress(true); return true; }
static bool writeOtaChunk(const uint8_t* data,size_t length) { if(!Update.isRunning() || otaState!=OtaState::Receiving){abortOta("chunk_without_active_update");return false;} if(!data || !length){abortOta("empty_chunk");return false;} if(otaReceivedSize+length>otaExpectedSize){abortOta("chunk_exceeds_expected_size");return false;} size_t written=Update.write(const_cast<uint8_t*>(data),length); if(written!=length){otaReceivedSize+=written;abortOta("write_failed");return false;} otaReceivedSize+=written;sendOtaProgress(false);return true; }
static bool finishOta() { if(otaState!=OtaState::Receiving || !Update.isRunning()){abortOta("end_without_active_update");return false;} if(otaReceivedSize!=otaExpectedSize){abortOta("size_mismatch");return false;} if(!Update.end(true)){abortOta("validation_failed");return false;} if(!Update.isFinished()){abortOta("image_not_finished");return false;} otaState=OtaState::Success;otaProgress=100;otaLastError=ESP_OK;otaCompletedAt=millis();sendOtaText("{\"type\":\"ota\",\"state\":\"success\",\"progress\":100,\"rebooting\":true}\n");Serial.println("[OTA] SUCCESS - image validated and boot partition switched");return true; }

static int16_t psxAxisToHid(uint8_t v){return(int16_t)((uint32_t(v)*32767U+127U)/255U);}
static void sendConfigHello(){bleConfigSendText("{\"type\":\"hello\",\"device\":\"PSXCore\",\"protocol\":7,\"transport\":\"shared-gatt\",\"hid\":true,\"liveState\":true,\"ota\":\"ready\",\"otaSupported\":true}\n");}
static void sendInfo(){char m[320];snprintf(m,sizeof(m),"{\"type\":\"info\",\"device\":\"%s\",\"version\":\"%s\",\"hardware\":\"%s\",\"build\":\"%s\",\"protocol\":7,\"transport\":\"shared-gatt\",\"hid\":true,\"config\":true,\"liveState\":true,\"ota\":\"ready\",\"otaSupported\":true}\n",BLE_DEVICE_NAME,PSXCORE_FIRMWARE_VERSION,PSXCORE_HARDWARE,PSXCORE_BUILD);bleConfigSendText(m);}
static void sendSettings(){bleConfigSendText("{\"type\":\"settings\",\"analogControl\":true,\"sleepMinutes\":5}\n");}
static void handleCommand(const uint8_t* data,size_t length){if(!data||!length)return;String command;command.reserve(length);for(size_t i=0;i<length;i++)command+=(char)data[i];command.trim();if(command=="PING")bleConfigSendText("PONG\n");else if(command=="HELLO"||command=="INFO")sendInfo();else if(command=="GET_STATE")bleConfigNotifyControllerState(true);else if(command=="GET_SETTINGS")sendSettings();else if(command=="SET_ANALOG"){psxEnableAnalogMode();bleConfigSendText("{\"type\":\"result\",\"command\":\"SET_ANALOG\",\"ok\":true}\n");}else bleConfigSendText("{\"type\":\"error\",\"error\":\"unknown_command\"}\n");}
static void handleOtaControl(const uint8_t* data,size_t length){if(!data||!length)return;String command;command.reserve(length);for(size_t i=0;i<length;i++)command+=(char)data[i];command.trim();if(command=="OTA_INFO")sendOtaInfo();else if(command.startsWith("OTA_BEGIN:")){String s=command.substring(strlen("OTA_BEGIN:"));s.trim();char* end=nullptr;unsigned long v=strtoul(s.c_str(),&end,10);if(!s.length()||end==s.c_str()||*end!='\0')abortOta("invalid_begin_command");else beginOta((size_t)v);}else if(command=="OTA_END")finishOta();else if(command=="OTA_RESET"){if(Update.isRunning())Update.abort();resetOtaState();sendOtaInfo();}else sendOtaText("{\"type\":\"ota\",\"state\":\"error\",\"reason\":\"unknown_control_command\"}\n");}
static void handleOtaData(const uint8_t* data,size_t length){if(data&&length)writeOtaChunk(data,length);}

static bool nimbleHostReady(){NimBLEServer* server=NimBLEDevice::getServer();return server!=nullptr;}

static void configureSharedAdvertising(){
  if(!nimbleHostReady()){advertisingConfigured=false;Serial.println("[BLE] Advertising WAITING FOR NIMBLE HOST");return;}
  NimBLEAdvertising* advertising=NimBLEDevice::getAdvertising();
  if(!advertising){advertisingConfigured=false;Serial.println("[BLE] Advertising ERROR: object unavailable");return;}
  if(advertising->isAdvertising()) advertising->stop();
  advertising->reset();
  NimBLEAdvertisementData advertisingData;
  advertisingData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advertisingData.addServiceUUID(HID_SERVICE_UUID);
  advertising->setAdvertisementData(advertisingData);
  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(BLE_DEVICE_NAME);
  advertising->setScanResponseData(scanResponseData);
  advertising->setAppearance(0x03C4);
  advertisingConfigured=true;
  bool started=advertising->start();
  delay(50);
  bool active=started&&advertising->isAdvertising();
  Serial.printf("[BLE] Shared advertising: %s\n",active?"ON":"FAILED");
  if(active){Serial.printf("[BLE] DISCOVERABLE NAME: %s\n",BLE_DEVICE_NAME);Serial.println("[BLE] ONE DEVICE: HID gamepad + PSXCore custom GATT");}
}

static void ensureAdvertising(){
  if(!nimbleHostReady()) return;
  NimBLEAdvertising* advertising=NimBLEDevice::getAdvertising();
  if(!advertising) return;
  if(!advertisingConfigured){configureSharedAdvertising();return;}
  if(!advertising->isAdvertising()){bool started=advertising->start();Serial.printf("[BLE] Advertising restart: %s\n",(started&&advertising->isAdvertising())?"OK":"FAILED");}
}

static void tryInitPsxCoreGatt(){
  if(configReady || !configInitPending) return;
  PsxCoreGattCallbacks callbacks={handleCommand,handleOtaControl,handleOtaData};
  if(psxCoreGattBegin(callbacks)){configReady=true;configInitPending=false;Serial.println("[BLE] PSXCore custom GATT: READY on shared BLE server");Serial.printf("[OTA] Custom GATT OTA: READY, next slot=%lu bytes\n",(unsigned long)otaAvailableSpace());}
}

static void updatePsxButtons(uint32_t b){static const struct{uint8_t psxBit;uint8_t hidButton;}m[]={{15,BUTTON_1},{14,BUTTON_2},{13,BUTTON_3},{12,BUTTON_4},{10,BUTTON_5},{11,BUTTON_6},{8,BUTTON_7},{9,BUTTON_8},{1,BUTTON_9},{2,BUTTON_10},{0,BUTTON_11},{3,BUTTON_12}};for(const auto&e:m){if(b&(1UL<<e.psxBit))bleGamepad.press(e.hidButton);else bleGamepad.release(e.hidButton);}bool u=b&(1UL<<4),r=b&(1UL<<5),d=b&(1UL<<6),l=b&(1UL<<7);if(u&&r)bleGamepad.setHat1(HAT_UP_RIGHT);else if(r&&d)bleGamepad.setHat1(HAT_DOWN_RIGHT);else if(d&&l)bleGamepad.setHat1(HAT_DOWN_LEFT);else if(l&&u)bleGamepad.setHat1(HAT_UP_LEFT);else if(u)bleGamepad.setHat1(HAT_UP);else if(r)bleGamepad.setHat1(HAT_RIGHT);else if(d)bleGamepad.setHat1(HAT_DOWN);else if(l)bleGamepad.setHat1(HAT_LEFT);else bleGamepad.setHat1(HAT_CENTERED);}

void ble_init(){
  BleGamepadConfiguration c;
  c.setAutoReport(false);
  c.setButtonCount(16);
  c.setHatSwitchCount(1);
  resetOtaState();
  configReady=false;
  configInitPending=true;
  advertisingConfigured=false;
  lastConfigInitAttempt=0;
  lastAdvertisingCheck=0;
  bleGamepad.begin(&c);
  NimBLEDevice::setDeviceName(BLE_DEVICE_NAME);
  Serial.println("[BLE] HID service: READY");
  Serial.println("[BLE] Shared BLE server architecture: HID + custom PSXCore GATT");
  Serial.println("[BLE] Waiting for NimBLE host sync before advertising");
}

void ble_send_report(){
  if(!configReady && configInitPending && (uint32_t)(millis()-lastConfigInitAttempt)>=250){lastConfigInitAttempt=millis();tryInitPsxCoreGatt();}
  if((uint32_t)(millis()-lastAdvertisingCheck)>=500){lastAdvertisingCheck=millis();ensureAdvertising();}
  if(otaState==OtaState::Success && otaCompletedAt && (uint32_t)(millis()-otaCompletedAt)>=1500){Serial.println("[OTA] RESTART NOW");delay(50);ESP.restart();return;}

  // Custom companion GATT is independent from the HID connection. Keep its
  // controller-state path alive even when Android is not connected as HID.
  if(configReady) bleConfigNotifyControllerState();

  bool connected=bleGamepad.isConnected();
  debugStatusBLEState(connected);
  if(connected!=lastConnected){
    Serial.printf("[BLE] HID %s\n",connected?"CONNECTED":"DISCONNECTED");
    lastConnected=connected;
    if(connected&&configReady)sendConfigHello();
    if(!connected)ensureAdvertising();
  }

  if(!connected) return;
  updatePsxButtons(controllerState.buttons);
  bleGamepad.setLeftThumb(psxAxisToHid(controllerState.lx),psxAxisToHid(controllerState.ly));
  bleGamepad.setRightThumb(psxAxisToHid(controllerState.rx),psxAxisToHid(controllerState.ry));
  bleGamepad.sendReport();
  debugStatusBLEUpdate();
}

void bleGamepadBegin(){ble_init();debugStatusBLEState(false);}
void bleGamepadUpdate(){ble_send_report();}
bool bleConfigIsReady(){return configReady&&psxCoreGattIsReady();}
void bleConfigSend(const uint8_t*d,size_t l){if(configReady&&d&&l)psxCoreGattSendResponse(d,l);}
void bleConfigSendText(const char*t){if(t)bleConfigSend(reinterpret_cast<const uint8_t*>(t),strlen(t));}
void bleConfigNotifyControllerState(bool force){if(!configReady)return;const ControllerState&s=controllerState;bool changed=s.buttons!=lastStateButtons||s.lx!=lastStateLx||s.ly!=lastStateLy||s.rx!=lastStateRx||s.ry!=lastStateRy;if(!force&&!changed)return;char m[160];snprintf(m,sizeof(m),"{\"type\":\"state\",\"buttons\":%lu,\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u}\n",(unsigned long)s.buttons,s.lx,s.ly,s.rx,s.ry);psxCoreGattSendStateText(m);lastStateButtons=s.buttons;lastStateLx=s.lx;lastStateLy=s.ly;lastStateRx=s.rx;lastStateRy=s.ry;}
