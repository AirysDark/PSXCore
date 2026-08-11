#include "controller_state.h"
#include "psx_buttons.h"

void decodePSXPacket(uint8_t *packet, ControllerState &state) {
  uint16_t buttons = ~(packet[3] | (packet[4] << 8));

  state.buttons = buttons;

  state.lx = packet[5];
  state.ly = packet[6];
  state.rx = packet[7];
  state.ry = packet[8];
}
