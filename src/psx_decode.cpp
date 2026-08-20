#include <stdint.h>
#include "controller_state.h"

// PS2 0x42 response decoder.
//
// Digital mode (ID 0x41):
//   0 FF, 1 ID, 2 5A, 3 buttonsLo, 4 buttonsHi
//
// Analog mode (ID 0x73/0x79):
//   0 FF, 1 ID, 2 5A, 3 buttonsLo, 4 buttonsHi,
//   5 RX, 6 RY, 7 LX, 8 LY
//
// PS2 button bits are active-low, so the decoded state exposes pressed
// buttons as 1 bits.

void decodePSXPacket(uint8_t *packet, ControllerState &state)
{
    if (packet[0] != 0xFF) return;

    state.buttons = ~(static_cast<uint16_t>(packet[3]) |
                      (static_cast<uint16_t>(packet[4]) << 8));

    // The wire order is RX, RY, LX, LY -- do not confuse it with the
    // logical left/right ordering used by the BLE gamepad API.
    state.rx = packet[5];
    state.ry = packet[6];
    state.lx = packet[7];
    state.ly = packet[8];
}
