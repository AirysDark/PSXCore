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

// Queue outgoing notifications. Frames are copied and emitted serially by
// psxCoreGattProcess() so a later setValue() cannot overwrite an earlier frame.
void psxCoreGattProcess();

void psxCoreGattSendResponse(const uint8_t* data, size_t length);
void psxCoreGattSendResponseText(const char* text);
void psxCoreGattSendState(const uint8_t* data, size_t length);
void psxCoreGattSendStateText(const char* text);
void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length);
void psxCoreGattSendOtaStatusText(const char* text);

// Compatibility API used by the existing BLE controller layer. These wrappers
// deliberately use the newline-framed PSXCore v7 transport so Android can
// reassemble fragmented notifications without mixing adjacent responses.
void bleConfigSendText(const char* text);
void bleConfigNotifyControllerState(bool force = false);
