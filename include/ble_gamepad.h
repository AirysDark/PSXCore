#pragma once

#include <Arduino.h>

void bleGamepadBegin();
void bleGamepadUpdate();

// Android companion/configuration channel. Uses the Nordic UART Service
// already hosted by the same NimBLE server as the HID gamepad.
bool bleConfigIsReady();
void bleConfigSend(const uint8_t* data, size_t length);
void bleConfigSendText(const char* text);
