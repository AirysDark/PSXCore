#pragma once

// PSX button mapping to HID buttons
#define BTN_SELECT 0x0001
#define BTN_L3     0x0002
#define BTN_R3     0x0004
#define BTN_START  0x0008
#define BTN_UP     0x0010
#define BTN_RIGHT  0x0020
#define BTN_DOWN   0x0040
#define BTN_LEFT   0x0080
#define BTN_L2     0x0100
#define BTN_R2     0x0200
#define BTN_L1     0x0400
#define BTN_R1     0x0800
#define BTN_TRIANGLE 0x1000
#define BTN_CIRCLE   0x2000
#define BTN_CROSS    0x4000
#define BTN_SQUARE   0x8000

struct PSXState {
  uint16_t buttons;
  uint8_t leftX;
  uint8_t leftY;
  uint8_t rightX;
  uint8_t rightY;
};
