#include <Arduino.h>
#include "psxcore/pins.h"
#include "psxcore/controller_state.h"

void psxcoreBegin(){
  pinMode(PSX_DATA, INPUT_PULLUP);
  pinMode(PSX_COMMAND, OUTPUT);
  pinMode(PSX_ATTENTION, OUTPUT);
  pinMode(PSX_CLOCK, OUTPUT);
  pinMode(PSX_ACK, INPUT_PULLUP);
}
