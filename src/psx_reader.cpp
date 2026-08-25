// PSXCore ESP32-S3 PS2 controller reader

#include <Arduino.h>
#include "psx_reader.h"
#include "psx_protocol.h"
#include "psx_decode.h"
#include "pins.h"
#include "controller_state.h"
#include "debug_status.h"
#include "analog_button.h"

static bool rawDebugEnabled = false;

static bool validControllerId(uint8_t id) {
  return id == 0x41 || id == 0x73 || id == 0x79;
}

void psxSetRawDebugEnabled(bool enabled) {
  rawDebugEnabled = enabled;
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
  uint8_t response0 = psxTransferByte(0x01);
  bool ackSeen = psxLastTransferAcked();

  uint8_t response1 = psxTransferByte(0x42);
  ackSeen |= psxLastTransferAcked();

  uint8_t response2 = psxTransferByte(0x00);
  ackSeen |= psxLastTransferAcked();

  if (controllerId != nullptr) *controllerId = response1;
  if (ackSeenOut != nullptr) *ackSeenOut = ackSeen;

  return response0 == 0xFF &&
         validControllerId(response1) &&
         response2 == 0x5A;
}

bool psxProbeController(uint8_t* controllerId) {
  bool ackSeen = false;
  uint8_t id = 0x00;

  for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
    digitalWrite(psxPins.command, HIGH);
    digitalWrite(psxPins.clock, HIGH);
    digitalWrite(psxPins.attention, HIGH);
    delayMicroseconds(50);

    digitalWrite(psxPins.attention, LOW);
    delayMicroseconds(20);

    bool valid = probeOnce(&id, &ackSeen);

    digitalWrite(psxPins.attention, HIGH);
    digitalWrite(psxPins.command, HIGH);
    digitalWrite(psxPins.clock, HIGH);
    delayMicroseconds(30);

    if (valid) {
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

  // 0x42 response lengths:
  //   ID 0x41 = digital mode: FF ID 5A buttonsLo buttonsHi
  //   ID 0x73/0x79 = analog mode: FF ID 5A buttonsLo buttonsHi RX RY LX LY
  uint8_t packet[9] = {0};
  bool ackSeen = false;

  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  delayMicroseconds(30);
  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

  for (uint8_t i = 0; i < sizeof(packet); ++i) {
    packet[i] = psxTransferByte(i == 0 ? 0x01 :
                                i == 1 ? 0x42 :
                                0x00);
    ackSeen |= psxLastTransferAcked();
  }

  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);

  // Continue reading immediately from boot, but do not interleave the first
  // diagnostic packets with asynchronous NimBLE startup/advertising output.
  if (rawDebugEnabled && diagnosticTransactions < 12) {
    Serial.printf("[PSX RAW %lu] ACK=%s",
                  (unsigned long)diagnosticTransactions,
                  ackSeen ? "YES" : "NO");
    for (uint8_t value : packet) Serial.printf(" %02X", value);
    Serial.println();
    diagnosticTransactions++;
  }

  if (packet[0] != 0xFF || !validControllerId(packet[1]) || packet[2] != 0x5A) {
    debugStatusPSXState(false);
    return;
  }

  analogButtonUpdate(packet[1]);

  if (packet[1] == 0x41) {
    packet[5] = 0x80;
    packet[6] = 0x80;
    packet[7] = 0x80;
    packet[8] = 0x80;
  }

  decodePSXPacket(packet, controllerState);
  debugStatusPSXPacket();
  debugStatusPSXState(true);
}
