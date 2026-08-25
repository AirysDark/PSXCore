#pragma once

#include <Arduino.h>

typedef void (*PsxCoreGattRxCallback)(const uint8_t* data, size_t length);

typedef struct {
    PsxCoreGattRxCallback command;
    PsxCoreGattRxCallback otaControl;
    PsxCoreGattRxCallback otaData;
} PsxCoreGattCallbacks;

bool psxCoreGattBegin(const PsxCoreGattCallbacks& callbacks);
bool psxCoreGattIsReady();

// Refresh BLE advertising after the custom PSXCore service is registered.
// HID and PSXCore GATT remain services on the same physical BLE device.
bool psxCoreGattRefreshAdvertising();

// Compatibility hook for the existing BLE loop. Direct notifications are sent
// immediately, so there is no outgoing frame queue to drain.
void psxCoreGattProcess();

void psxCoreGattSendResponse(const uint8_t* data, size_t length);
void psxCoreGattSendResponseText(const char* text);
void psxCoreGattSendState(const uint8_t* data, size_t length);
void psxCoreGattSendStateText(const char* text);
void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length);
void psxCoreGattSendOtaStatusText(const char* text);

// Compatibility API used by the existing BLE controller layer. These wrappers
// use the newline-framed PSXCore v7 transport so Android receives complete
// logical messages without the custom 20-byte queue/chunk layer.
void bleConfigSendText(const char* text);

// The default argument belongs only in ble_gamepad.h. Keeping this declaration
// without a default avoids a duplicate default argument when both headers are
// included by ble_nimble.cpp.
void bleConfigNotifyControllerState(bool force);
