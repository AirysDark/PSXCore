#pragma once
#include <Arduino.h>
#include <PSXController.h>

struct PSXInputState { uint16_t buttons=0; uint8_t hat=8; int8_t leftX=0,leftY=0,rightX=0,rightY=0; uint8_t l2=0,r2=0; };
struct PSXInputConfig { uint8_t deadZone=12; bool invertLeftY=true; bool invertRightY=true; };
class PSXInputMapper { public: explicit PSXInputMapper(const PSXInputConfig& config=PSXInputConfig{}); PSXInputState map(const PSXControllerState& raw) const; void setConfig(const PSXInputConfig& config); const PSXInputConfig& config() const; private: PSXInputConfig _config; int8_t mapAxis(uint8_t value,bool invert) const; uint8_t mapHat(uint16_t buttons) const; };
