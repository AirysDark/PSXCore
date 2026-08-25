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

// PSXCore Companion GATT v7 contract.
// These UUIDs MUST stay identical to android/.../ProtocolConstants.kt.
static const char* SERVICE_UUID     = "7a4f0000-0000-4f50-5358-434f52450001";
static const char* COMMAND_UUID     = "7a4f0000-0000-4f50-5358-434f52450002";
static const char* RESPONSE_UUID    = "7a4f0000-0000-4f50-5358-434f52450003";
static const char* STATE_UUID       = "7a4f0000-0000-4f50-5358-434f52450004";
static const char* OTA_CONTROL_UUID = "7a4f0000-0000-4f50-5358-434f52450005";
static const char* OTA_DATA_UUID    = "7a4f0000-0000-4f50-5358-434f52450006";
static const char* OTA_STATUS_UUID  = "7a4f0000-0000-4f50-5358-434f52450007";

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.command) return;
        std::string value = characteristic->getValue();
        if (value.empty()) return;
        Serial.printf("[PSX-GATT] COMMAND RX: %u bytes\n", (unsigned)value.size());
        rxCallbacks.command(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.otaControl) return;
        std::string value = characteristic->getValue();
        if (value.empty()) return;
        Serial.printf("[PSX-GATT] OTA CONTROL RX: %u bytes\n", (unsigned)value.size());
        rxCallbacks.otaControl(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (!characteristic || !rxCallbacks.otaData) return;
        std::string value = characteristic->getValue();
        if (value.empty()) return;
        rxCallbacks.otaData(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }
};

static CommandCallbacks commandCallbacks;
static OtaControlCallbacks otaControlCallbacks;
static OtaDataCallbacks otaDataCallbacks;

bool psxCoreGattRefreshAdvertising() {
    return true;
}

bool psxCoreGattBegin(const PsxCoreGattCallbacks& callbacks) {
    rxCallbacks = callbacks;
    if (gattReady) return true;
    if (gattInitializing) return false;

    NimBLEServer* server = NimBLEDevice::getServer();
    if (!server) {
        Serial.println("[PSX-GATT] NimBLE server not ready yet");
        return false;
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    bool wasAdvertising = advertising && advertising->isAdvertising();
    if (wasAdvertising) {
        Serial.println("[PSX-GATT] Stopping advertising before GATT database update");
        advertising->stop();
        delay(20);
    }

    gattInitializing = true;
    Serial.println("[PSX-GATT] Creating PSXCore companion service v7");
    Serial.printf("[PSX-GATT] Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("[PSX-GATT] Command UUID: %s\n", COMMAND_UUID);

    psxService = server->createService(SERVICE_UUID);
    if (!psxService) {
        gattInitializing = false;
        Serial.println("[PSX-GATT] ERROR: service creation failed");
        return false;
    }

    commandChar = psxService->createCharacteristic(
        COMMAND_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    responseChar = psxService->createCharacteristic(
        RESPONSE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    stateChar = psxService->createCharacteristic(
        STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    otaControlChar = psxService->createCharacteristic(
        OTA_CONTROL_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    otaDataChar = psxService->createCharacteristic(
        OTA_DATA_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    otaStatusChar = psxService->createCharacteristic(
        OTA_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    if (!commandChar || !responseChar || !stateChar || !otaControlChar ||
        !otaDataChar || !otaStatusChar) {
        gattInitializing = false;
        Serial.println("[PSX-GATT] ERROR: characteristic creation failed");
        return false;
    }

    commandChar->setCallbacks(&commandCallbacks);
    otaControlChar->setCallbacks(&otaControlCallbacks);
    otaDataChar->setCallbacks(&otaDataCallbacks);

    psxService->start();

    gattReady = true;
    gattInitializing = false;

    Serial.println("[PSX-GATT] Service registered successfully");
    Serial.println("[PSX-GATT] GATT CONTRACT: Android + firmware UUIDs synchronized");
    Serial.println("[PSX-GATT] COMMAND -> command callback");
    Serial.println("[PSX-GATT] OTA_CONTROL -> OTA control callback");
    Serial.println("[PSX-GATT] OTA_DATA -> raw firmware callback");
    Serial.println("[PSX-GATT] RESPONSE/STATE/OTA_STATUS notifications active");
    return true;
}

bool psxCoreGattIsReady() { return gattReady; }

static void sendCharacteristic(
    NimBLECharacteristic* characteristic, const uint8_t* data, size_t length) {
    if (!gattReady || !characteristic || !data || !length) return;
    characteristic->setValue(data, length);
    characteristic->notify();
}

// RESPONSE, STATE and OTA_STATUS are newline-framed text streams on Android.
// Enforce the delimiter here so every notification path uses the same framing,
// including callers that use the uint8_t*/length APIs instead of *Text().
static void sendDataFrame(
    NimBLECharacteristic* characteristic,
    const uint8_t* data,
    size_t length) {
    if (!data || !length) return;

    if (data[length - 1] == '\n') {
        sendCharacteristic(characteristic, data, length);
        return;
    }

    std::string framed(reinterpret_cast<const char*>(data), length);
    framed.push_back('\n');
    sendCharacteristic(
        characteristic,
        reinterpret_cast<const uint8_t*>(framed.data()),
        framed.size()
    );
}

static void sendTextFrame(NimBLECharacteristic* characteristic, const char* text) {
    if (!text) return;
    sendDataFrame(
        characteristic,
        reinterpret_cast<const uint8_t*>(text),
        strlen(text)
    );
}

void psxCoreGattSendResponse(const uint8_t* data, size_t length) {
    sendDataFrame(responseChar, data, length);
}

void psxCoreGattSendResponseText(const char* text) {
    sendTextFrame(responseChar, text);
}

void psxCoreGattSendState(const uint8_t* data, size_t length) {
    sendDataFrame(stateChar, data, length);
}

void psxCoreGattSendStateText(const char* text) {
    sendTextFrame(stateChar, text);
}

void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length) {
    sendDataFrame(otaStatusChar, data, length);
}

void psxCoreGattSendOtaStatusText(const char* text) {
    sendTextFrame(otaStatusChar, text);
}
