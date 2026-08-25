#pragma once

#include <Arduino.h>

void bleGamepadBegin();
void bleGamepadUpdate();

// Android companion/configuration channel hosted by the same NimBLE server
// as the HID gamepad through the custom PSXCore GATT service.
bool bleConfigIsReady();
void bleConfigSend(const uint8_t* data, size_t length);
void bleConfigSendText(const char* text);

// Send the current controller state as a custom PSXCore GATT STATE notification.
// force=true sends a snapshot even when the controller state has not changed.
void bleConfigNotifyControllerState(bool force = false);