#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <PSXInputMapper.h>
class PSXBLEGamepad : public NimBLEServerCallbacks { public: PSXBLEGamepad()=default; bool begin(const char* deviceName="PSXCore Controller"); void update(const PSXInputState& input); bool send(); bool connected() const; void onConnect(NimBLEServer* server,NimBLEConnInfo& connInfo) override; void onDisconnect(NimBLEServer* server,NimBLEConnInfo& connInfo,int reason) override; private: struct __attribute__((packed)) Report { uint16_t buttons; uint8_t hat; int8_t lx,ly,rx,ry; uint8_t l2,r2; }; Report _report{}; NimBLEServer* _server=nullptr; NimBLECharacteristic* _input=nullptr; bool _connected=false; };
