#include <Arduino.h>
#include <Preferences.h>
#include "pins.h"

PsxPins psxPins = {
  PSX_DEFAULT_DATA,
  PSX_DEFAULT_COMMAND,
  PSX_DEFAULT_ATTENTION,
  PSX_DEFAULT_CLOCK,
  PSX_DEFAULT_ACK
};

static Preferences preferences;
static bool preferencesOpen = false;

void psxPinsBegin() {
  // Open read/write so the namespace is created on a fresh device. The old
  // read-only open produced an NVS NOT_FOUND warning when no saved mapping
  // existed yet.
  if (preferences.begin("psxcore", false)) {
    preferencesOpen = true;

    if (preferences.getBool("valid", false)) {
      psxPins.data = preferences.getInt("data", PSX_DEFAULT_DATA);
      psxPins.command = preferences.getInt("cmd", PSX_DEFAULT_COMMAND);
      psxPins.attention = preferences.getInt("att", PSX_DEFAULT_ATTENTION);
      psxPins.clock = preferences.getInt("clk", PSX_DEFAULT_CLOCK);
      psxPins.ack = preferences.getInt("ack", PSX_DEFAULT_ACK);
    }

    preferences.end();
    preferencesOpen = false;
  }
}

void psxSetPins(const PsxPins& pins, bool persist) {
  psxPins = pins;

  if (!persist) {
    return;
  }

  if (preferences.begin("psxcore", false)) {
    preferencesOpen = true;
    preferences.putBool("valid", true);
    preferences.putInt("data", psxPins.data);
    preferences.putInt("cmd", psxPins.command);
    preferences.putInt("att", psxPins.attention);
    preferences.putInt("clk", psxPins.clock);
    preferences.putInt("ack", psxPins.ack);
    preferences.end();
    preferencesOpen = false;
  }
}

void psxResetPins() {
  psxPins = {
    PSX_DEFAULT_DATA,
    PSX_DEFAULT_COMMAND,
    PSX_DEFAULT_ATTENTION,
    PSX_DEFAULT_CLOCK,
    PSX_DEFAULT_ACK
  };

  if (preferences.begin("psxcore", false)) {
    preferences.clear();
    preferences.end();
  }
}
