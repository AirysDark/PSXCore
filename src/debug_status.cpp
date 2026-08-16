#include <Arduino.h>
#include "debug_status.h"

static uint32_t psxPackets = 0;
static uint32_t bleUpdates = 0;
static bool psxDetected = false;
static bool bleConnected = false;

void debugStatusInit() {
  Serial.println("[DEBUG] status ready");
}

void debugStatusPSXPacket() {
  psxPackets++;
}

void debugStatusBLEUpdate() {
  bleUpdates++;
}

void debugStatusPSXState(bool detected) {
  psxDetected = detected;
}

void debugStatusBLEState(bool connected) {
  bleConnected = connected;
}

void debugStatusLoop() {
  static uint32_t last = 0;

  if (millis() - last >= 1000) {
    last = millis();

    Serial.print("[STATUS] uptime=");
    Serial.print(millis() / 1000);
    Serial.print("s PSX packets=");
    Serial.print(psxPackets);
    Serial.print(" BLE updates=");
    Serial.print(bleUpdates);
    Serial.print(" PSX=");
    Serial.print(psxDetected ? "OK" : "WAIT");
    Serial.print(" BLE=");
    Serial.println(bleConnected ? "CONNECTED" : "ADVERTISING");
  }
}
