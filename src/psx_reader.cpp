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
  static uint32_t noResponseReports = 0;
  uint8_t packet[9] = {0};
  bool ackSeen = false;

  // Give the controller a clean bus-idle interval before selecting it.
  digitalWrite(PSX_COMMAND, HIGH);
  digitalWrite(PSX_CLOCK, HIGH);
  digitalWrite(PSX_ATTENTION, HIGH);
  delayMicroseconds(20);
  digitalWrite(PSX_ATTENTION, LOW);
  delayMicroseconds(20);

  // Standard PSX/PS2 poll command.
  psxTransferByte(0x01);
  ackSeen |= psxLastTransferAcked();

  psxTransferByte(0x42);
  ackSeen |= psxLastTransferAcked();

  for (int i = 0; i < 9; i++) {
    packet[i] = psxTransferByte(0x00);
    ackSeen |= psxLastTransferAcked();
  }

  digitalWrite(PSX_ATTENTION, HIGH);
  digitalWrite(PSX_COMMAND, HIGH);
  digitalWrite(PSX_CLOCK, HIGH);

  // Print the first few complete transactions for electrical/protocol
  // diagnosis, including whether the controller ever asserted ACK.
  if (diagnosticTransactions < 10) {
    Serial.printf("[PSX RAW %lu] ACK=%s", (unsigned long)diagnosticTransactions,
                  ackSeen ? "YES" : "NO");
    for (uint8_t value : packet) {
      Serial.printf(" %02X", value);
    }
    Serial.println();
    diagnosticTransactions++;
  }

  // A completely idle PSX data bus reads as FF. Treat FF/FF as no response,
  // rather than an unknown controller, and report it at a controlled rate.
  if (!ackSeen || (packet[0] == 0xFF && packet[1] == 0xFF)) {
    debugStatusPSXState(false);
    noResponseReports++;
    if ((noResponseReports % 250) == 1) {
      Serial.printf("[PSX] No controller response (ACK=%s, header=%02X ID=%02X)\n",
                    ackSeen ? "YES" : "NO", packet[0], packet[1]);
    }
    return;
  }

  if (packet[0] != 0xFF) {
    debugStatusPSXState(false);
    return;
  }

  // Standard controller IDs include 0x41 (digital), 0x73 and 0x79
  // (analog/DualShock variants). Keep genuinely unknown IDs visible without
  // flooding the serial monitor.
  if (packet[1] != 0x41 && packet[1] != 0x73 && packet[1] != 0x79) {
    if ((noResponseReports++ % 250) == 1) {
      Serial.printf("[PSX] Unknown controller ID: %02X\n", packet[1]);
    }
    debugStatusPSXState(false);
    return;
  }

  noResponseReports = 0;
  decodePSXPacket(packet, controllerState);
  debugStatusPSXState(true);
}
