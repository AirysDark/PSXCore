#pragma once
#include <stdint.h>

struct ControllerState {
  uint32_t buttons;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
};

extern ControllerState controllerState;
