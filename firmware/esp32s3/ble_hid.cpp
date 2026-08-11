#include "controller_state.h"

// BLE HID bridge layer
// Converts ControllerState into a generic gamepad report.

struct HIDReport {
  uint16_t buttons;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
};

HIDReport makeReport(const ControllerState &state) {
  HIDReport report;
  report.buttons = state.buttons;
  report.lx = state.lx;
  report.ly = state.ly;
  report.rx = state.rx;
  report.ry = state.ry;
  return report;
}
