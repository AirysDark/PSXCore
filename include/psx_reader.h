#pragma once

#include <stdint.h>

void psxBegin();
bool psxProbeController(uint8_t* controllerId = nullptr);
void psxReadController();

// PSX polling always continues during boot. Raw diagnostic printing is
// controlled separately so asynchronous BLE startup messages stay together
// before the first PSX RAW lines are emitted.
void psxSetRawDebugEnabled(bool enabled);
