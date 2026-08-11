#include "button_map.h"

// Bluetooth HID report generation layer

struct BLEReport {
  uint32_t buttons;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
};

BLEReport report;

void updateBLEReport() {
  // TODO: connect to ESP32-S3 BLE HID library
  // Send controller state over Bluetooth.
}
