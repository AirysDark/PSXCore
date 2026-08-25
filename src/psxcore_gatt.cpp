#include <Arduino.h>
#include <NimBLEDevice.h>
#include "psxcore_gatt.h"

static NimBLEService* psxService = nullptr;
static NimBLECharacteristic* commandChar = nullptr;
static NimBLECharacteristic* responseChar = nullptr;
static NimBLECharacteristic* stateChar = nullptr;
static NimBLECharacteristic* otaControlChar = nullptr;
static NimBLECharacteristic* otaDataChar = nullptr;
static NimBLECharacteristic* otaStatusChar = nullptr;
static PsxCoreGattCallbacks rxCallbacks = { nullptr, nullptr, nullptr };
static bool gattReady = false;
static bool gattInitializing = false;

static const char* SERVICE_UUID     = "7a4f0000-0000-4f50-5358-434f52450001";
static const char* COMMAND_UUID     = "7a4f0001-0000-4f50-5358-434f52450001";
static const char* RESPONSE_UUID    = "7a4f0002-0000-4f50-5358-434f52450001";
static const char* STATE_UUID       = "7a4f0003-0000-4f50-5358-434f52450001";
static const char* OTA_CONTROL_UUID = "7a4f0004-0000-4f50-5358-434f52450001";
static const char* OTA_DATA_UUID    = "7a4f0005-0000-4f50-5358-434f52450001";
static const char* OTA_STATUS_UUID  = "7a4f0006-0000-4f50-5358-434f52450001";

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.command) return;
        std::string value = characteristic->getValue();
        if (!value.empty()) rxCallbacks.command(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.otaControl) return;
        std::string value = characteristic->getValue();
        if (!value.empty()) rxCallbacks.otaControl(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.otaData) return;
        std::string value = characteristic->getValue();
        if (!value.empty()) rxCallbacks.otaData(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

static CommandCallbacks commandCallbacks;
static OtaControlCallbacks otaControlCallbacks;
static OtaDataCallbacks otaDataCallbacks;

bool psxCoreGattBegin(const PsxCoreGattCallbacks& callbacks) {
    rxCallbacks = callbacks;
    if (gattReady) return true;
    if (gattInitializing) return false;

    NimBLEServer* server = NimBLEDevice::getServer();
    if (!server) {
        Serial.println("[PSX-GATT] NimBLE server not ready yet");
        return false;
    }

    gattInitializing = true;
    psxService = server->createService(SERVICE_UUID);
    if (!psxService) {
        gattInitializing = false;
        Serial.println("[PSX-GATT] ERROR: service creation failed");
        return false;
    }

    commandChar = psxService->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    responseChar = psxService->createCharacteristic(RESPONSE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    stateChar = psxService->createCharacteristic(STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    otaControlChar = psxService->createCharacteristic(OTA_CONTROL_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    otaDataChar = psxService->createCharacteristic(OTA_DATA_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    otaStatusChar = psxService->createCharacteristic(OTA_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    if (!commandChar || !responseChar || !stateChar || !otaControlChar || !otaDataChar || !otaStatusChar) {
        gattInitializing = false;
        Serial.println("[PSX-GATT] ERROR: characteristic creation failed");
        return false;
    }

    commandChar->setCallbacks(&commandCallbacks);
    otaControlChar->setCallbacks(&otaControlCallbacks);
    otaDataChar->setCallbacks(&otaDataCallbacks);

    // NimBLE-Arduino automatically manages the CCCD (0x2902) for NOTIFY characteristics.
    // Do not manually create descriptors here.
    gattReady = true;
    gattInitializing = false;

    Serial.println("[PSX-GATT] Custom PSXCore service READY");
    Serial.println("[PSX-GATT] COMMAND -> command callback");
    Serial.println("[PSX-GATT] OTA_CONTROL -> OTA control callback");
    Serial.println("[PSX-GATT] OTA_DATA -> raw firmware callback");
    Serial.println("[PSX-GATT] RESPONSE/STATE/OTA_STATUS notifications active");
    return true;
}

bool psxCoreGattIsReady() { return gattReady; }

static void sendCharacteristic(NimBLECharacteristic* characteristic, const uint8_t* data, size_t length) {
    if (!gattReady || !characteristic || !data || !length) return;
    characteristic->setValue(data, length);
    characteristic->notify();
}

void psxCoreGattSendResponse(const uint8_t* data, size_t length) { sendCharacteristic(responseChar, data, length); }
void psxCoreGattSendResponseText(const char* text) { if (text) psxCoreGattSendResponse(reinterpret_cast<const uint8_t*>(text), strlen(text)); }
void psxCoreGattSendState(const uint8_t* data, size_t length) { sendCharacteristic(stateChar, data, length); }
void psxCoreGattSendStateText(const char* text) { if (text) psxCoreGattSendState(reinterpret_cast<const uint8_t*>(text), strlen(text)); }
void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length) { sendCharacteristic(otaStatusChar, data, length); }
void psxCoreGattSendOtaStatusText(const char* text) { if (text) psxCoreGattSendOtaStatus(reinterpret_cast<const uint8_t*>(text), strlen(text)); }
