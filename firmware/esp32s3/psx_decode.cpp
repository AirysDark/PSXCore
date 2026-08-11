#include <stdint.h>
#include "controller_state.h"

// PSX response decoder
// Packet format:
// 0: 0xFF header
// 1: ID
// 2: length
// 3-4: button low/high
// 5-8: analog values

void decodePSXPacket(uint8_t *packet, ControllerState &state)
{
    if(packet[0] != 0xFF) return;

    state.buttons = ~(packet[3] | (packet[4] << 8));

    state.leftX  = packet[5];
    state.leftY  = packet[6];
    state.rightX = packet[7];
    state.rightY = packet[8];
}
