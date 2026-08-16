#include <Arduino.h>
#include "psx_pin_sweep.h"
#include "pins.h"
#include "psx_protocol.h"

namespace {

static const int SWEEP_PINS[5] = {4, 5, 6, 7, 8};

bool validControllerId(uint8_t id) {
  return id == 0x41 || id == 0x73 || id == 0x79;
}

void resetSweepPins() {
  for (int pin : SWEEP_PINS) {
    pinMode(pin, INPUT_PULLUP);
  }
}

bool probeMapping(const PsxPins& candidate, uint8_t& controllerId) {
  psxSetPins(candidate, false);
  psxProtocolInit();

  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);
  digitalWrite(psxPins.attention, HIGH);
  delayMicroseconds(50);

  // Standard PSX transaction:
  // Host: 01 42 00
  // Pad : FF ID 5A
  // The previous implementation incorrectly checked the 3rd returned
  // byte as FF and the 4th as the controller ID, so all 120 permutations
  // were rejected even when the wiring was correct.
  digitalWrite(psxPins.attention, LOW);
  delayMicroseconds(20);

  uint8_t response0 = psxTransferByte(0x01);
  bool ack0 = psxLastTransferAcked();

  uint8_t response1 = psxTransferByte(0x42);
  bool ack1 = psxLastTransferAcked();

  uint8_t response2 = psxTransferByte(0x00);
  bool ack2 = psxLastTransferAcked();

  controllerId = response1;

  digitalWrite(psxPins.attention, HIGH);
  digitalWrite(psxPins.command, HIGH);
  digitalWrite(psxPins.clock, HIGH);

  return ack0 && ack1 && ack2 &&
         response0 == 0xFF &&
         validControllerId(controllerId) &&
         response2 == 0x5A;
}

bool nextPermutation(int* values, int count) {
  int i = count - 2;
  while (i >= 0 && values[i] >= values[i + 1]) {
    --i;
  }
  if (i < 0) {
    return false;
  }

  int j = count - 1;
  while (values[j] <= values[i]) {
    --j;
  }

  int temp = values[i];
  values[i] = values[j];
  values[j] = temp;

  for (int left = i + 1, right = count - 1; left < right; ++left, --right) {
    temp = values[left];
    values[left] = values[right];
    values[right] = temp;
  }
  return true;
}

} // namespace

bool psxPinSweep() {
  Serial.println();
  Serial.println("[PIN SWEEP] PSX response not detected");
  Serial.println("[PIN SWEEP] Scanning GPIO 4,5,6,7,8");
  Serial.println("[PIN SWEEP] Testing pin-role permutations...");

  int permutation[5] = {4, 5, 6, 7, 8};
  uint32_t attempts = 0;
  uint8_t controllerId = 0;

  do {
    PsxPins candidate = {
      permutation[0], // DATA
      permutation[1], // COMMAND
      permutation[2], // ATTENTION
      permutation[3], // CLOCK
      permutation[4]  // ACK
    };

    ++attempts;
    if (probeMapping(candidate, controllerId)) {
      Serial.println();
      Serial.println("[PIN SWEEP] VALID PSX WIRING FOUND");
      Serial.printf("[PIN SWEEP] DATA=%d CMD=%d ATT=%d CLK=%d ACK=%d\n",
                    candidate.data,
                    candidate.command,
                    candidate.attention,
                    candidate.clock,
                    candidate.ack);
      Serial.printf("[PIN SWEEP] Controller ID=%02X\n", controllerId);
      Serial.println("[PIN SWEEP] Saving corrected pin mapping");

      psxSetPins(candidate, true);
      psxProtocolInit();
      return true;
    }
  } while (nextPermutation(permutation, 5));

  resetSweepPins();
  psxSetPins({
    PSX_DEFAULT_DATA,
    PSX_DEFAULT_COMMAND,
    PSX_DEFAULT_ATTENTION,
    PSX_DEFAULT_CLOCK,
    PSX_DEFAULT_ACK
  }, false);

  Serial.printf("[PIN SWEEP] No valid wiring found after %lu combinations\n",
                (unsigned long)attempts);
  Serial.println("[PIN SWEEP] Keeping default mapping 4/5/6/7/8");
  return false;
}
