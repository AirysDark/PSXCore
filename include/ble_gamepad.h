#pragma once

#include <Arduino.h>

// Bluetooth HID gamepad only. The former Android companion/configuration
// GATT service has been removed completely.
void bleGamepadBegin();
void bleGamepadUpdate();
