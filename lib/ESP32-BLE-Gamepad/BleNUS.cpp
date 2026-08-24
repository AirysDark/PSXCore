#include "BleNUS.h"
#include <NimBLEDevice.h>
#include "NimBLELog.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "BleNUS"
#else
#include "esp_log.h"
static const char *LOG_TAG = "BleNUS";
#endif

BleNUS::BleNUS(NimBLEServer* existingServer)
    : pServer(existingServer), pService(nullptr), pTxCharacteristic(nullptr), pRxCharacteristic(nullptr), dataReceivedCallback(nullptr) {}

BleNUS::~BleNUS() {
    end();
}

void BleNUS::begin() {
    delay(1000);  // Give some time for other services to complete their business

    if (!pServer) {
        Serial.println("No pServer");
        NIMBLE_LOGD(LOG_TAG, "No existing pServer available");
        return;
    }

    NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
    NIMBLE_LOGD(LOG_TAG, "Stopping main NimBLE server from advertising (shouldn't be at this stage if you set delayAdvertising to true)");
    pAdvertising->stop();

    NIMBLE_LOGD(LOG_TAG, "Creating Nordic UART Service");
    pService = pServer->createService(NUS_SERVICE_UUID);

    NIMBLE_LOGD(LOG_TAG, "Adding Nordic UART Service TX and RX characteristics");
    pTxCharacteristic = pService->createCharacteristic(NUS_TX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::NOTIFY);
    pRxCharacteristic = pService->createCharacteristic(NUS_RX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
    NIMBLE_LOGD(LOG_TAG, "Registering Nordic UART Service callbacks");
    pRxCharacteristic->setCallbacks(this);

    // NimBLE-Arduino 2.x starts all services when the server starts.
    // NimBLEService::start() is deprecated and has no effect.

    NimBLEAdvertisementData scanResponseData;
    scanResponseData.addServiceUUID(pService->getUUID());
    pAdvertising->setScanResponseData(scanResponseData);

    NIMBLE_LOGD(LOG_TAG, "Main NimBLE server advertising started!");
    pAdvertising->start();
}

void BleNUS::end() {
    if (pService) {
        // Nothing I can think of
    }
}

void BleNUS::sendData(const uint8_t* data, size_t length) {
    if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
        pTxCharacteristic->setValue(data, length);
        pTxCharacteristic->notify();
    }
}
