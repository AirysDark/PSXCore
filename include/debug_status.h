#pragma once

#include <stdint.h>

void debugStatusInit();
void debugStatusLoop();
void debugStatusPSXPacket();
void debugStatusBLEUpdate();

void debugStatusPSXState(bool detected);
void debugStatusBLEState(bool connected);
