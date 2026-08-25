#pragma once

// Enables and locks the PS2 controller in analog mode. The return state is
// retained so other subsystems (BLE companion/state telemetry) can report the
// actual last-known mode without re-running the controller configuration flow.
void psxEnableAnalogMode();
bool psxIsAnalogMode();
