#pragma once
#include <Arduino.h>
struct PSXControllerState { bool connected=false; bool analogMode=false; uint16_t buttons=0; uint8_t leftX=128,leftY=128,rightX=128,rightY=128; uint8_t pressureUp=0,pressureRight=0,pressureDown=0,pressureLeft=0,pressureTriangle=0,pressureCircle=0,pressureCross=0,pressureSquare=0,pressureL1=0,pressureR1=0,pressureL2=0,pressureR2=0; };
enum class PSXButton:uint8_t { Select=0,L3,R3,Start,Up,Right,Down,Left,L2,R2,L1,R1,Triangle,Circle,Cross,Square };
struct PSXControllerPins { int8_t data=-1,command=-1,attention=-1,clock=-1; };
class PSXController { public: PSXController()=default; explicit PSXController(const PSXControllerPins& pins); bool begin(); bool begin(const PSXControllerPins& pins); void end(); bool update(); const PSXControllerState& state() const; PSXControllerState read(); bool connected() const; bool analogMode() const; bool pressed(PSXButton button) const; uint8_t pressure(PSXButton button) const; const PSXControllerPins& pins() const; private: PSXControllerPins _pins{}; PSXControllerState _state{}; bool _started=false; void resetState(); uint8_t transfer(uint8_t value); bool poll(); };
