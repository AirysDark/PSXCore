#pragma once

// Optional rumble driver interface
// Purple PS2 7V wire is intentionally unused.

#define RUMBLE_PIN -1

void setRumble(uint8_t strength);
