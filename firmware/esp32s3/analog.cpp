#include "button_map.h"

// Convert PSX analog values to BLE HID range
uint8_t convertAnalog(uint8_t value) {
  return value;
}

void updateAnalog(PSXState &state, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry) {
  state.leftX = convertAnalog(lx);
  state.leftY = convertAnalog(ly);
  state.rightX = convertAnalog(rx);
  state.rightY = convertAnalog(ry);
}
