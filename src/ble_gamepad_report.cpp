#include "controller_state.h"

// Bluetooth HID report builder
// Converts PSX state into generic gamepad format

struct HIDReport {
    uint16_t buttons;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
};

HIDReport buildHIDReport(const ControllerState &state)
{
    HIDReport report;
    report.buttons = state.buttons;
    report.lx = state.leftX;
    report.ly = state.leftY;
    report.rx = state.rightX;
    report.ry = state.rightY;
    return report;
}
