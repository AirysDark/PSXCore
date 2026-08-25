#pragma once

#include <stdint.h>

// System-level handler for the PS2 controller's physical ANALOG button.
//
// On a standard DualShock/DualShock 2, the ANALOG button is handled inside
// the controller and is not exposed as a normal bit in the 0x42 button mask.
// We therefore detect a button activation from the controller mode transition:
//   0x41      = digital mode
//   0x73/0x79 = analog mode
//
// This gives PSXCore a single event hook today while keeping the higher-level
// system-button API ready for future wake/discovery actions.

enum class AnalogButtonEvent : uint8_t {
  None,
  ModeChanged,
  ShortPress,
  LongPress
};

void analogButtonInit(uint8_t initialControllerId);
void analogButtonUpdate(uint8_t controllerId);
AnalogButtonEvent analogButtonTakeEvent();

bool analogButtonIsAnalogMode();
bool analogButtonWasPressed();

// Reserved for future features. A physical long-press cannot be measured on
// standard PS2 protocol hardware because the ANALOG switch is not reported as
// a continuously-held button bit.
bool analogButtonLongPressAvailable();
