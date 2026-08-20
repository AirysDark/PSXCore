#include <Arduino.h>
#include "psx_config.h"
#include "pins.h"
#include "psx_protocol.h"
#include "psx_reader.h"

// Configure a DualShock/DualShock 2 controller for locked analog mode.
//
// Standard sequence:
//   01 43 00 01 00 00 00 00 00  -> enter configuration mode
//   01 44 00 01 03 00 00 00 00  -> enable + lock analog mode
//   01 43 00 00 5A 5A 5A 5A 5A  -> leave configuration mode
//
// A controller in analog mode reports ID 0x73 during a normal 0x42 poll.
// ACK is recorded for diagnostics but is not used as the sole success
// criterion because this hardware is already returning valid DATA bytes.

static void finishConfigTransaction() {
  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  delayMicroseconds(100);
}

static void sendConfigCommand(const uint8_t *command, size_t length) {
  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

  for (size_t i = 0; i < length; ++i) {
    psxTransferByte(command[i]);
  }

  finishConfigTransaction();
  delayMicroseconds(100);
}

bool psx_enable_analog_mode() {
  static const uint8_t enterConfig[] = {
      0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t enableAnalog[] = {
      0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t exitConfig[] = {
      0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};

  Serial.println("[PSX] Entering controller configuration mode...");
  sendConfigCommand(enterConfig, sizeof(enterConfig));

  Serial.println("[PSX] Enabling + locking analog mode...");
  sendConfigCommand(enableAnalog, sizeof(enableAnalog));

  Serial.println("[PSX] Leaving controller configuration mode...");
  sendConfigCommand(exitConfig, sizeof(exitConfig));

  // Give the controller time to apply the mode before polling it.
  delay(2);

  uint8_t id = 0;
  const bool valid = psxProbeController(&id);

  Serial.printf("[PSX] Analog mode verification: ID=%02X (%s)\n",
                id,
                (valid && id == 0x73) ? "ANALOG" :
                (valid && id == 0x41) ? "DIGITAL" : "NO RESPONSE");

  return valid && id == 0x73;
}
