#pragma once

#include <stdint.h>
#include "controller_state.h"

constexpr uint32_t PSXCORE_IDLE_SLEEP_MS = 5UL * 60UL * 1000UL;

void powerManagerBegin(const ControllerState& initialState);
void powerManagerUpdate(const ControllerState& state, bool analogEvent);
bool powerManagerIsSleeping();
void powerManagerWake();
