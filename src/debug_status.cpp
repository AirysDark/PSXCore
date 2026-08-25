#include <Arduino.h>
#include "debug_status.h"

static uint32_t psxPackets = 0;
static uint32_t bleUpdates = 0;
static bool psxDetected = false;
static bool bleConnected = false;

static constexpr uint32_t BLE_HEARTBEAT_INTERVAL_MS = 120000UL;

void debugStatusInit() {
  Serial.println("[DEBUG] status ready");
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
  static bool previousConnected = false;

  if (connected != previousConnected) {
    Serial.printf("[BLE] App %s\n", connected ? "connected" : "disconnected");
    previousConnected = connected;
  }

  bleConnected = connected;
}

void debugStatusLoop() {
  static uint32_t lastHeartbeat = 0;

  // Keep normal serial output quiet. Only print a heartbeat every two
  // minutes while the Android app is actually connected over BLE.
  if (!bleConnected) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastHeartbeat < BLE_HEARTBEAT_INTERVAL_MS) {
    return;
  }

  lastHeartbeat = now;

  Serial.printf("[BLE] Connection heartbeat: CONNECTED | uptime=%lus PSX=%s packets=%lu BLE updates=%lu\n",
                static_cast<unsigned long>(now / 1000UL),
                psxDetected ? "OK" : "WAIT",
                static_cast<unsigned long>(psxPackets),
                static_cast<unsigned long>(bleUpdates));
}
