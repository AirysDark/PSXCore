// PSXCore ESP32-S3 PS2 controller reader

#include <Arduino.h>
#include "psx_reader.h"
#include "psx_protocol.h"
#include "psx_decode.h"
#include "pins.h"
#include "controller_state.h"
#include "debug_status.h"

static bool validControllerId(uint8_t id) {
  return id == 0x41 || id == 0x73 || id == 0x79;
}

void psxBegin() {
  psxPinsBegin();
  psxProtocolInit();
  debugStatusPSXState(false);

  Serial.println("[PSX] Bus initialized");
  Serial.printf("[PSX] DATA=%d CMD=%d ATT=%d CLK=%d ACK=%d\n",
                psxPins.data,
                psxPins.command,
                psxPins.attention,
                psxPins.clock,
                psxPins.ack);
}

bool psxProbeController(uint8_t* controllerId) {
  uint8_t packet[2] = {0xFF, 0xFF};
  bool ackSeen = false;

  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  delayMicroseconds(30);

  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

  psxTransferByte(0x01);
  ackSeen |= psxLastTransferAcked();

  psxTransferByte(0x42);
  ackSeen |= psxLastTransferAcked();

  packet[0] = psxTransferByte(0x00);
  ackSeen |= psxLastTransferAcked();

  packet[1] = psxTransferByte(0x00);
  ackSeen |= psxLastTransferAcked();

  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);

  const bool valid = ackSeen && packet[0] == 0xFF && validControllerId(packet[1]);

  if (controllerId != nullptr) {
    *controllerId = packet[1];
  }

  return valid;
}

void psxReadController() {
  static uint32_t diagnosticTransactions = 0;
  static uint32_t noResponseReports = 0;
  uint8_t packet[9] = {0};
  bool ackSeen = false;

  // Give the controller a clean bus-idle interval before selecting it.
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  delayMicroseconds(20);
  digitalWrite(psxPins.attention, LOW);
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

  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);

  // Diagnostic output is deliberately bounded. Normal operation must never
  // flood the serial monitor.
  if (diagnosticTransactions < 3) {
    Serial.printf("[PSX RAW %lu] ACK=%s", (unsigned long)diagnosticTransactions,
                  ackSeen ? "YES" : "NO");
    for (uint8_t value : packet) {
      Serial.printf(" %02X", value);
    }
    Serial.println();
    diagnosticTransactions++;
  }

  // A completely idle PSX data bus reads as FF. Treat FF/FF as no response,
  // rather than an unknown controller.
  if (!ackSeen || (packet[0] == 0xFF && packet[1] == 0xFF)) {
    debugStatusPSXState(false);
    noResponseReports++;
    return;
  }

  if (packet[0] != 0xFF) {
    debugStatusPSXState(false);
    return;
  }

  if (!validControllerId(packet[1])) {
    if (noResponseReports == 0) {
      Serial.printf("[PSX] Unknown controller ID: %02X\n", packet[1]);
    }
    noResponseReports++;
    debugStatusPSXState(false);
    return;
  }

  noResponseReports = 0;
  decodePSXPacket(packet, controllerState);
  debugStatusPSXState(true);
}
