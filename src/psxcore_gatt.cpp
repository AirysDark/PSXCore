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

struct PendingFrame {
    std::string data;
    size_t offset = 0;
};

static std::deque<PendingFrame> responseQueue;
static std::deque<PendingFrame> stateQueue;
static std::deque<PendingFrame> otaQueue;
static constexpr size_t MAX_PENDING_FRAMES = 32;
// Safe on the minimum BLE ATT MTU (23 bytes -> 20-byte payload).
// Android reassembles the newline-framed logical message.
static constexpr size_t SAFE_NOTIFY_CHUNK = 20;

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
    const bool wasAdvertising = advertising && advertising->isAdvertising();
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

    responseQueue.clear();
    stateQueue.clear();
    otaQueue.clear();
    gattReady = true;
    gattInitializing = false;

    Serial.println("[PSX-GATT] Service registered successfully");
    Serial.println("[PSX-GATT] GATT CONTRACT: Android + firmware UUIDs synchronized");
    Serial.println("[PSX-GATT] Outgoing notifications: SERIALIZED + NEWLINE FRAMED + MTU-SAFE CHUNKED");
    return true;
}

bool psxCoreGattIsReady() { return gattReady; }

static void enqueueFrame(std::deque<PendingFrame>& queue, const uint8_t* data, size_t length, bool replaceLatest) {
    if (!gattReady || !data || !length) return;

    PendingFrame frame;
    frame.data.assign(reinterpret_cast<const char*>(data), length);
    frame.offset = 0;

    if (replaceLatest) {
        // Never replace a frame that is already being transmitted. Replacing
        // queue.back() unconditionally used to reset offset to zero when the
        // queue contained only the in-flight state frame, causing repeated
        // prefixes such as 20/73 -> 40/73 -> 20/73 and corrupt JSON on Android.
        // Keep at most one in-flight frame plus one newest pending state.
        if (queue.empty()) {
            queue.push_back(std::move(frame));
        } else if (queue.front().offset > 0) {
            if (queue.size() == 1) {
                queue.push_back(std::move(frame));
            } else {
                queue.back() = std::move(frame);
            }
        } else {
            // Nothing from the current frame has been sent yet, so it is safe
            // to replace it with the latest controller state.
            queue.front() = std::move(frame);
        }
        return;
    }

    if (queue.size() >= MAX_PENDING_FRAMES) {
        Serial.println("[PSX-GATT] TX queue full; dropping oldest frame");
        queue.pop_front();
    }
    queue.push_back(std::move(frame));
}

static void enqueueTextFrame(std::deque<PendingFrame>& queue, const char* text, bool replaceLatest) {
    if (!text) return;
    const size_t length = strlen(text);
    if (!length) return;
    std::string frame(text, length);
    if (frame.back() != '\n') frame.push_back('\n');
    enqueueFrame(queue, reinterpret_cast<const uint8_t*>(frame.data()), frame.size(), replaceLatest);
}

static bool sendNextFrame(NimBLECharacteristic* characteristic, std::deque<PendingFrame>& queue, const char* label) {
    if (!gattReady || !characteristic || queue.empty()) return false;

    PendingFrame& frame = queue.front();
    if (frame.offset >= frame.data.size()) {
        queue.pop_front();
        return false;
    }

    const size_t remaining = frame.data.size() - frame.offset;
    const size_t chunkLength = remaining > SAFE_NOTIFY_CHUNK ? SAFE_NOTIFY_CHUNK : remaining;

    characteristic->setValue(
        reinterpret_cast<const uint8_t*>(frame.data.data() + frame.offset),
        chunkLength
    );

    if (!characteristic->notify()) {
        Serial.printf(
            "[PSX-GATT] TX %s pending: notify not accepted; keeping frame at %u/%u bytes\n",
            label,
            (unsigned)frame.offset,
            (unsigned)frame.data.size()
        );
        return false;
    }

    frame.offset += chunkLength;
    const bool complete = frame.offset >= frame.data.size();

    Serial.printf(
        "[PSX-GATT] TX %s chunk delivered: %u bytes (%u/%u)%s\n",
        label,
        (unsigned)chunkLength,
        (unsigned)frame.offset,
        (unsigned)frame.data.size(),
        complete ? " COMPLETE" : ""
    );

    if (complete) queue.pop_front();
    return true;
}

void psxCoreGattProcess() {
    if (sendNextFrame(responseChar, responseQueue, "RESPONSE")) return;
    if (sendNextFrame(stateChar, stateQueue, "STATE")) return;
    sendNextFrame(otaStatusChar, otaQueue, "OTA_STATUS");
}

void psxCoreGattSendResponse(const uint8_t* data, size_t length) { enqueueFrame(responseQueue, data, length, false); }
void psxCoreGattSendResponseText(const char* text) { enqueueTextFrame(responseQueue, text, false); }
void psxCoreGattSendState(const uint8_t* data, size_t length) { enqueueFrame(stateQueue, data, length, true); }
void psxCoreGattSendStateText(const char* text) { enqueueTextFrame(stateQueue, text, true); }
void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length) { enqueueFrame(otaQueue, data, length, false); }
void psxCoreGattSendOtaStatusText(const char* text) { enqueueTextFrame(otaQueue, text, false); }
