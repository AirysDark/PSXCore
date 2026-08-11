#pragma once
#include <stdint.h>

struct ControllerState {
  uint32_t buttons = 0;
  uint8_t lx = 128;
  uint8_t ly = 128;
  uint8_t rx = 128;
  uint8_t ry = 128;
};

extern ControllerState controllerState;
