// PSXCore ESP32-S3 PS2 controller reader

#include <Arduino.h>
#include "psx_reader.h"
#include "psx_protocol.h"
#include "psx_decode.h"
#include "pins.h"
#include "controller_state.h"
#include "debug_status.h"

void psxBegin() {
  psxProtocolInit();
  debugStatusPSXState(false);

  Serial.println("[PSX] Bus initialized");
  Serial.printf("[PSX] DATA=%d CMD=%d ATT=%d CLK=%d ACK=%d\n",
                PSX_DATA, PSX_COMMAND, PSX_ATTENTION, PSX_CLOCK, PSX_ACK);
}

void psxReadController() {
  static uint32_t diagnosticTransactions = 0;
  uint8_t packet[9] = {0};

  digitalWrite(PSX_ATTENTION, LOW);

  // Standard PSX/PS2 poll command.
  psxTransferByte(0x01);
  psxTransferByte(0x42);

  for (int i = 0; i < 9; i++) {
    packet[i] = psxTransferByte(0x00);
  }

  digitalWrite(PSX_ATTENTION, HIGH);

  // Print the first few raw responses so electrical/protocol problems are
  // visible without flooding the serial monitor during normal operation.
  if (diagnosticTransactions < 10) {
    Serial.printf("[PSX RAW %lu]", (unsigned long)diagnosticTransactions);
    for (uint8_t value : packet) {
      Serial.printf(" %02X", value);
    }
    Serial.println();
    diagnosticTransactions++;
  }

  // 0xFF is the normal PSX response header. Do NOT reject it.
  if (packet[0] != 0xFF) {
    debugStatusPSXState(false);
    return;
  }

  // Standard controller IDs include 0x41 (digital), 0x73 and 0x79
  // (analog/DualShock variants). Keep unknown IDs visible for diagnosis.
  if (packet[1] != 0x41 && packet[1] != 0x73 && packet[1] != 0x79) {
    Serial.printf("[PSX] Unknown controller ID: %02X\n", packet[1]);
    debugStatusPSXState(false);
    return;
  }

  decodePSXPacket(packet, controllerState);
  debugStatusPSXState(true);
}
