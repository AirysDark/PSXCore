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

static bool probeOnce(uint8_t* controllerId, bool* ackSeenOut) {
  uint8_t response0 = 0xFF;
  uint8_t response1 = 0xFF;
  uint8_t response2 = 0xFF;
  bool ackSeen = false;

  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  delayMicroseconds(50);

  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

  response0 = psxTransferByte(0x01);
  ackSeen |= psxLastTransferAcked();
  response1 = psxTransferByte(0x42);
  ackSeen |= psxLastTransferAcked();
  response2 = psxTransferByte(0x00);
  ackSeen |= psxLastTransferAcked();

  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  delayMicroseconds(30);

  if (controllerId != nullptr) {
    *controllerId = response1;
  }
  if (ackSeenOut != nullptr) {
    *ackSeenOut = ackSeen;
  }

  // DATA is authoritative for controller detection. ACK is useful diagnostic
  // information, but some PSX adapters/controllers do not expose it reliably.
  return response0 == 0xFF && validControllerId(response1) && response2 == 0x5A;
}

bool psxProbeController(uint8_t* controllerId) {
  bool ackSeen = false;
  uint8_t id = 0x00;

  for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
    if (probeOnce(&id, &ackSeen)) {
      if (controllerId != nullptr) *controllerId = id;
      Serial.printf("[PSX] Controller response VALID (ID=%02X, ACK=%s, attempt=%u)\n",
                    id, ackSeen ? "YES" : "NO", attempt);
      return true;
    }

    delay(2);
  }

  if (controllerId != nullptr) *controllerId = id;
  return false;
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
  delayMicroseconds(30);
  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

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

  // Do not require ACK for a valid controller. The controller ID and packet
  // structure are the primary indication that DATA/CMD/ATT/CLK are working.
  if (packet[0] != 0xFF || !validControllerId(packet[1])) {
    debugStatusPSXState(false);
    noResponseReports++;
    return;
  }

  noResponseReports = 0;
  decodePSXPacket(packet, controllerState);
  debugStatusPSXState(true);
}
