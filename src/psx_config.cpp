#include <Arduino.h>
#include "pins.h"

uint8_t psxTransferByte(uint8_t value);

bool psx_enable_analog_mode() {
  digitalWrite(PSX_ATTENTION, LOW);

  psxTransferByte(0x01);
  psxTransferByte(0x43);
  psxTransferByte(0x00);
  psxTransferByte(0x01);
  psxTransferByte(0x03);
  psxTransferByte(0x00);
  psxTransferByte(0x00);
  psxTransferByte(0x00);

  digitalWrite(PSX_ATTENTION, HIGH);
  return true;
}
