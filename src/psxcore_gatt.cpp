#include <Arduino.h>
#include <NimBLEDevice.h>
#include <deque>
#include <string>

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

// Queueing is deliberate. NimBLE characteristic values are mutable; issuing
// setValue()+notify() repeatedly in the same callback/loop can let a later
// frame replace an earlier frame before it reaches the ATT notification path.
static std::deque<std::string> responseQueue;
static std::deque<std::string> stateQueue;
static std::deque<std::string> otaQueue;
static constexpr size_t MAX_PENDING_FRAMES = 32;

// PSXCore Companion GATT v7 contract.
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
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!advertising) return false;
    if (advertising->isAdvertising()) return true;
    const bool started = advertising->start();
    return started && advertising->isAdvertising();
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
    psxService->start();

    responseQueue.clear();
    stateQueue.clear();
    otaQueue.clear();
    gattReady = true;
    gattInitializing = false;

    Serial.println("[PSX-GATT] Service registered successfully");
    Serial.println("[PSX-GATT] GATT CONTRACT: Android + firmware UUIDs synchronized");
    Serial.println("[PSX-GATT] Outgoing notifications: SERIALIZED + RETRY ON DELIVERY FAILURE");
    return true;
}

bool psxCoreGattIsReady() { return gattReady; }

static void enqueueFrame(std::deque<std::string>& queue, const uint8_t* data, size_t length, bool replaceLatest) {
    if (!gattReady || !data || !length) return;
    std::string frame(reinterpret_cast<const char*>(data), length);

    if (replaceLatest) {
        if (queue.empty()) queue.push_back(std::move(frame));
        else queue.back() = std::move(frame);
        return;
    }

    if (queue.size() >= MAX_PENDING_FRAMES) {
        Serial.println("[PSX-GATT] TX queue full; dropping oldest frame");
        queue.pop_front();
    }
    queue.push_back(std::move(frame));
}

static void enqueueTextFrame(std::deque<std::string>& queue, const char* text, bool replaceLatest) {
    if (!text) return;
    const size_t length = strlen(text);
    if (!length) return;
    std::string frame(text, length);
    if (frame.back() != '\n') frame.push_back('\n');
    enqueueFrame(queue, reinterpret_cast<const uint8_t*>(frame.data()), frame.size(), replaceLatest);
}

static bool sendNextFrame(NimBLECharacteristic* characteristic, std::deque<std::string>& queue, const char* label) {
    if (!gattReady || !characteristic || queue.empty()) return false;

    // Do not remove the frame until NimBLE confirms that the notification was
    // accepted. The previous implementation popped first, so a notification
    // failure silently lost GET_STATE and other messages while still printing
    // a misleading TX line.
    const std::string& frame = queue.front();
    characteristic->setValue(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());

    if (!characteristic->notify()) {
        Serial.printf("[PSX-GATT] TX %s pending: notify not accepted; keeping %u-byte frame queued\n",
                      label, (unsigned)frame.size());
        return false;
    }

    const size_t length = frame.size();
    queue.pop_front();
    Serial.printf("[PSX-GATT] TX %s delivered: %u bytes\n", label, (unsigned)length);
    return true;
}

void psxCoreGattProcess() {
    // Exactly one notification per main-loop pass. This prevents back-to-back
    // setValue()/notify() calls from overwriting a characteristic's pending value.
    if (sendNextFrame(responseChar, responseQueue, "RESPONSE")) return;
    if (sendNextFrame(stateChar, stateQueue, "STATE")) return;
    sendNextFrame(otaStatusChar, otaQueue, "OTA_STATUS");
}

void psxCoreGattSendResponse(const uint8_t* data, size_t length) {
    enqueueFrame(responseQueue, data, length, false);
}

void psxCoreGattSendResponseText(const char* text) {
    enqueueTextFrame(responseQueue, text, false);
}

void psxCoreGattSendState(const uint8_t* data, size_t length) {
    // State is latest-value-wins; stale joystick packets are not useful.
    enqueueFrame(stateQueue, data, length, true);
}

void psxCoreGattSendStateText(const char* text) {
    enqueueTextFrame(stateQueue, text, true);
}

void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length) {
    enqueueFrame(otaQueue, data, length, false);
}

void psxCoreGattSendOtaStatusText(const char* text) {
    enqueueTextFrame(otaQueue, text, false);
}
