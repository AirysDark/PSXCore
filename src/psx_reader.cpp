// PSXCore ESP32-S3 PS2 controller reader

#include <Arduino.h>
#include "psx_reader.h"
#include "psx_protocol.h"
#include "pins.h"
#include "controller_state.h"

void psxBegin() {
  psxProtocolInit();
}

void psxReadController() {
  // Poll transaction will update controllerState.
  // Kept separated from the transport layer so protocol timing
  // and controller mapping can be tested independently.
}
