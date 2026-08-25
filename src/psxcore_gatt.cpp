#include "psxcore_gatt.h"
#include <NimBLEDevice.h>
#include <deque>
#include <string>

namespace {

constexpr const char* SERVICE_UUID = "7a4f0000-0000-4f50-5358-434f52450001";
constexpr const char* COMMAND_UUID = "7a4f0000-0000-4f50-5358-434f52450002";
constexpr const char* RESPONSE_UUID = "7a4f0000-0000-4f50-5358-434f52450003";
constexpr const char* STATE_UUID = "7a4f0000-0000-4f50-5358-434f52450004";
constexpr const char* OTA_CONTROL_UUID = "7a4f0000-0000-4f50-5358-434f52450005";
constexpr const char* OTA_DATA_UUID = "7a4f0000-0000-4f50-5358-434f52450006";
constexpr const char* OTA_STATUS_UUID = "7a4f0000-0000-4f50-5358-434f52450007";

NimBLEServer* server = nullptr;
NimBLEService* psxService = nullptr;
NimBLECharacteristic* commandChar = nullptr;
NimBLECharacteristic* responseChar = nullptr;
NimBLECharacteristic* stateChar = nullptr;
NimBLECharacteristic* otaControlChar = nullptr;
NimBLECharacteristic* otaDataChar = nullptr;
NimBLECharacteristic* otaStatusChar = nullptr;

PsxCoreGattCallbacks registeredCallbacks{};
bool gattReady = false;
bool gattInitializing = false;

std::deque<std::string> responseQueue;
std::deque<std::string> stateQueue;
std::deque<std::string> otaQueue;

std::string frameText(const char* text) {
    if (!text) return "";
    std::string frame(text);
    if (frame.empty() || frame.back() != '\n') frame.push_back('\n');
    return frame;
}

void queueFrame(std::deque<std::string>& queue, const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    std::string frame(reinterpret_cast<const char*>(data), length);
    if (frame.empty() || frame.back() != '\n') frame.push_back('\n');
    queue.push_back(std::move(frame));
}

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const std::string value = characteristic->getValue();
        Serial.printf("[PSX-GATT] COMMAND RX: %u bytes\n", static_cast<unsigned>(value.size()));
        if (registeredCallbacks.command && !value.empty()) {
            registeredCallbacks.command(reinterpret_cast<const uint8_t*>(value.data()), value.size());
        }
    }
};

class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const std::string value = characteristic->getValue();
        Serial.printf("[PSX-GATT] OTA_CONTROL RX: %u bytes\n", static_cast<unsigned>(value.size()));
        if (registeredCallbacks.otaControl && !value.empty()) {
            registeredCallbacks.otaControl(reinterpret_cast<const uint8_t*>(value.data()), value.size());
        }
    }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const std::string value = characteristic->getValue();
        if (registeredCallbacks.otaData && !value.empty()) {
            registeredCallbacks.otaData(reinterpret_cast<const uint8_t*>(value.data()), value.size());
        }
    }
};

CommandCallbacks commandCallbacks;
OtaControlCallbacks otaControlCallbacks;
OtaDataCallbacks otaDataCallbacks;

bool emitNext(NimBLECharacteristic* characteristic, std::deque<std::string>& queue, const char* label) {
    if (!characteristic || queue.empty()) return false;
    if (characteristic->getSubscribedCount() == 0) return false;

    std::string frame = std::move(queue.front());
    queue.pop_front();
    characteristic->setValue(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
    characteristic->notify();
    Serial.printf("[PSX-GATT] TX %s: %u bytes\n", label, static_cast<unsigned>(frame.size()));
    return true;
}

} // namespace

bool psxCoreGattBegin(const PsxCoreGattCallbacks& callbacks) {
    if (gattReady) return true;
    if (gattInitializing) return false;

    gattInitializing = true;
    registeredCallbacks = callbacks;

    server = NimBLEDevice::getServer();
    if (!server) {
        gattInitializing = false;
        Serial.println("[PSX-GATT] ERROR: NimBLE server not ready yet");
        return false;
    }

    Serial.println("[PSX-GATT] Creating PSXCore companion service v7");
    Serial.printf("[PSX-GATT] Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("[PSX-GATT] Command UUID: %s\n", COMMAND_UUID);

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

    // NimBLE-Arduino starts services as part of the server. Calling start() here
    // is deprecated and has no effect, so deliberately do not call psxService->start().

    responseQueue.clear();
    stateQueue.clear();
    otaQueue.clear();
    gattReady = true;
    gattInitializing = false;

    Serial.println("[PSX-GATT] Service registered successfully");
    Serial.println("[PSX-GATT] GATT CONTRACT: Android + firmware UUIDs synchronized");
    Serial.println("[PSX-GATT] Outgoing notifications: SERIALIZED + NEWLINE FRAMED");
    return true;
}

bool psxCoreGattIsReady() {
    return gattReady;
}

bool psxCoreGattRefreshAdvertising() {
    if (!server || !gattReady) return false;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!advertising) return false;
    if (advertising->isAdvertising()) advertising->stop();
    return advertising->start();
}

void psxCoreGattProcess() {
    if (!gattReady) return;
    if (emitNext(responseChar, responseQueue, "RESPONSE")) return;
    if (emitNext(stateChar, stateQueue, "STATE")) return;
    emitNext(otaStatusChar, otaQueue, "OTA_STATUS");
}

void psxCoreGattSendResponse(const uint8_t* data, size_t length) {
    queueFrame(responseQueue, data, length);
}

void psxCoreGattSendResponseText(const char* text) {
    if (!text) return;
    responseQueue.push_back(frameText(text));
}

void psxCoreGattSendState(const uint8_t* data, size_t length) {
    queueFrame(stateQueue, data, length);
}

void psxCoreGattSendStateText(const char* text) {
    if (!text) return;
    stateQueue.push_back(frameText(text));
}

void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length) {
    queueFrame(otaQueue, data, length);
}

void psxCoreGattSendOtaStatusText(const char* text) {
    if (!text) return;
    otaQueue.push_back(frameText(text));
}

void bleConfigSendText(const char* text) {
    psxCoreGattSendResponseText(text);
}

void bleConfigNotifyControllerState(bool force) {
    (void)force;
    // The controller layer supplies the actual serialized state through the
    // compatibility transport elsewhere; this symbol exists for shared-header
    // compatibility and must not introduce a second default argument.
}
