// PSXCore ESP32-S3 PS2 controller reader

#include <Arduino.h>
#include "psx_reader.h"
#include "psx_protocol.h"
#include "psx_decode.h"
#include "pins.h"
#include "controller_state.h"

void psxBegin() {
  psxProtocolInit();
}

void psxReadController() {
  uint8_t packet[9] = {0};

  digitalWrite(PSX_ATTENTION, LOW);

  psxTransferByte(0x01);
  psxTransferByte(0x42);

  for (int i = 0; i < 9; i++) {
    packet[i] = psxTransferByte(0x00);
  }

  digitalWrite(PSX_ATTENTION, HIGH);

  decodePSXPacket(packet, controllerState);
}
