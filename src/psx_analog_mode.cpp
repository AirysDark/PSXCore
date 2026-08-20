#include "psx_analog_mode.h"
#include "psx_config.h"

// Compatibility entry point for code that uses the older analog-mode API.
// The actual controller configuration lives in psx_config.cpp so there is
// one authoritative implementation of the PS2 0x43/0x44/0x43 sequence.
void psxEnableAnalogMode() {
    (void)psx_enable_analog_mode();
}
