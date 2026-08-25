#pragma once

#include <Arduino.h>

typedef void (*PsxCoreGattRxCallback)(const uint8_t* data, size_t length);

bool psxCoreGattBegin(PsxCoreGattRxCallback callback);
bool psxCoreGattIsReady();
void psxCoreGattSendResponse(const uint8_t* data, size_t length);
void psxCoreGattSendResponseText(const char* text);
void psxCoreGattSendState(const uint8_t* data, size_t length);
void psxCoreGattSendStateText(const char* text);
void psxCoreGattSendOtaStatus(const uint8_t* data, size_t length);
void psxCoreGattSendOtaStatusText(const char* text);
