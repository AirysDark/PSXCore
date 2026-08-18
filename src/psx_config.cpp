#include <Arduino.h>
#include "psx_config.h"
#include "pins.h"
#include "psx_protocol.h"

bool psx_enable_analog_mode() {
  digitalWrite(psxPins.attention, LOW);

  psxTransferByte(0x01);
  psxTransferByte(0x43);
  psxTransferByte(0x00);
  psxTransferByte(0x01);
  psxTransferByte(0x03);
  psxTransferByte(0x00);
  psxTransferByte(0x00);
  psxTransferByte(0x00);

  digitalWrite(psxPins.attention, HIGH);

  return true;
}