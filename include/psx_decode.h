#pragma once

#include "controller_state.h"
#include <stdint.h>

void decodePSXPacket(uint8_t *packet, ControllerState &state);
