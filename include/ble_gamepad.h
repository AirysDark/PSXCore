#pragma once

#include <Arduino.h>

void bleGamepadBegin();
void bleGamepadUpdate();

// Android companion/configuration channel on the same NimBLE server as HID.
bool bleConfigIsReady();
void bleConfigSend(const uint8_t* data, size_t length);
void bleConfigSendText(const char* text);

// Send controller state. Force mode is used for explicit GET_STATE requests;
// normal mode only emits when the controller state changed.
void bleConfigNotifyControllerState(bool force = false);
