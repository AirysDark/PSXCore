#include <Arduino.h>

// PSX controller configuration commands
// Enables analog mode on compatible controllers.

bool psx_enable_analog_mode() {
  // TODO: send 0x43 configuration transaction.
  return true;
}
