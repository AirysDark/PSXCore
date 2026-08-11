#include <Arduino.h>

struct StickCalibration {
  int centerX = 128;
  int centerY = 128;
  int deadzone = 12;
};

StickCalibration leftStick;
StickCalibration rightStick;

int applyDeadzone(int value, int center, int zone) {
  if (abs(value - center) < zone) return center;
  return value;
}
