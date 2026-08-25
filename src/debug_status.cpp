#include <Arduino.h>
#include "debug_status.h"

static uint32_t psxPackets = 0;
static uint32_t bleUpdates = 0;
static bool psxDetected = false;
static bool bleConnected = false;

static constexpr uint32_t STATUS_INTERVAL_MS = 5000;

void debugStatusInit() {
  Serial.println("[DEBUG] status ready (5s interval)");
}

void debugStatusPSXPacket() {
  ++psxPackets;
}

void debugStatusBLEUpdate() {
  ++bleUpdates;
}

void debugStatusPSXState(bool detected) {
  psxDetected = detected;
}

void debugStatusBLEState(bool connected) {
  bleConnected = connected;
}

void debugStatusLoop() {
  static uint32_t lastReport = 0;
  static uint32_t lastPsxPackets = 0;
  static uint32_t lastBleUpdates = 0;

  const uint32_t now = millis();
  if (now - lastReport < STATUS_INTERVAL_MS) return;

  const uint32_t elapsedMs = now - lastReport;
  const uint32_t psxDelta = psxPackets - lastPsxPackets;
  const uint32_t bleDelta = bleUpdates - lastBleUpdates;

  lastReport = now;
  lastPsxPackets = psxPackets;
  lastBleUpdates = bleUpdates;

  const uint32_t psxRate = elapsedMs ? (psxDelta * 1000UL) / elapsedMs : 0;
  const uint32_t bleRate = elapsedMs ? (bleDelta * 1000UL) / elapsedMs : 0;

  Serial.printf("[STATUS] uptime=%lus PSX=%s packets=%lu (%luHz) BLE=%s updates=%lu (%luHz)\n",
                static_cast<unsigned long>(now / 1000UL),
                psxDetected ? "OK" : "WAIT",
                static_cast<unsigned long>(psxPackets),
                static_cast<unsigned long>(psxRate),
                bleConnected ? "CONNECTED" : "ADVERTISING",
                static_cast<unsigned long>(bleUpdates),
                static_cast<unsigned long>(bleRate));
}
