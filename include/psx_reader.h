#pragma once

#include <stdint.h>

void psxBegin();
bool psxProbeController(uint8_t* controllerId = nullptr);
void psxReadController();
