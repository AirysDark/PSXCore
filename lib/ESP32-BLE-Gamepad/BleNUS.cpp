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

BleNUS::~BleNUS() { end(); }

void BleNUS::begin() {
    delay(1000);
    if (!pServer) {
        Serial.println("No pServer");
        NIMBLE_LOGD(LOG_TAG, "No existing pServer available");
        return;
    }
    NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->stop();
    pService = pServer->createService(NUS_SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(NUS_TX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::NOTIFY);
    pRxCharacteristic = pService->createCharacteristic(NUS_RX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
    pRxCharacteristic->setCallbacks(this);
    // NimBLE 2.x starts services with the server; pService->start() is deprecated.
    NimBLEAdvertisementData scanResponseData;
    scanResponseData.addServiceUUID(pService->getUUID());
    pAdvertising->setScanResponseData(scanResponseData);
    pAdvertising->start();
}

void BleNUS::end() { if (pService) {} }

void BleNUS::sendData(const uint8_t* data, size_t length) {
    if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
        pTxCharacteristic->setValue(data, length);
        pTxCharacteristic->notify();
    }
}

void BleNUS::setDataReceivedCallback(void (*callback)(const uint8_t* data, size_t length)) { dataReceivedCallback = callback; }

void BleNUS::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (dataReceivedCallback) {
        std::string value = pCharacteristic->getValue();
        buffer += value;
        dataReceivedCallback((const uint8_t*)value.data(), value.length());
    }
}

size_t BleNUS::available() { return buffer.length(); }

int BleNUS::read() {
    if (buffer.length() > 0) {
        char c = buffer[0];
        buffer.erase(0, 1);
        return c;
    }
    return -1;
}

int BleNUS::peek() { return buffer.length() > 0 ? buffer[0] : -1; }
void BleNUS::flush() { buffer.clear(); }

void BleNUS::print(const char* str) { sendData((const uint8_t*)str, strlen(str)); }
void BleNUS::print(const String& str) { print(str.c_str()); }
void BleNUS::print(int i) { char buf[32]; itoa(i, buf, 10); print(buf); }
void BleNUS::print(long l) { char buf[32]; ltoa(l, buf, 10); print(buf); }
void BleNUS::print(unsigned long ul) { char buf[32]; ultoa(ul, buf, 10); print(buf); }
void BleNUS::print(float f, int digits) { char buf[32]; dtostrf(f, 6, digits, buf); print(buf); }
void BleNUS::print(double d, int digits) { print((float)d, digits); }
void BleNUS::print(char c) { char buf[2] = {c, '\0'}; print(buf); }

void BleNUS::println(const char* str) { print(str); print("\n"); }
void BleNUS::println(const String& str) { println(str.c_str()); }
void BleNUS::println(int i) { print(i); print("\n"); }
void BleNUS::println(long l) { print(l); print("\n"); }
void BleNUS::println(unsigned long ul) { print(ul); print("\n"); }
void BleNUS::println(float f, int digits) { print(f, digits); print("\n"); }
void BleNUS::println(double d, int digits) { print(d, digits); print("\n"); }
void BleNUS::println(char c) { print(c); print("\n"); }

void BleNUS::write(uint8_t byte) { print(byte); }
void BleNUS::write(const uint8_t *data, size_t size) { sendData(data, size); }
