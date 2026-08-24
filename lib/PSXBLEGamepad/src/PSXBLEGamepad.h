#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <PSXInputMapper.h>

class PSXBLEGamepad : public NimBLEServerCallbacks {
public:
    PSXBLEGamepad() = default;

    bool begin(const char* deviceName = "PSXCore Controller");
    void end();

    void update(const PSXInputState& input);
    bool send();
    bool send(const PSXInputState& input);

    bool connected() const;
    bool started() const;
    void setBatteryLevel(uint8_t percent);

    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override;

private:
    struct __attribute__((packed)) Report {
        uint16_t buttons;
        uint8_t hat;
        int8_t lx;
        int8_t ly;
        int8_t rx;
        int8_t ry;
        uint8_t l2;
        uint8_t r2;
    };

    static_assert(sizeof(Report) == 9, "PSX HID input report must be exactly 9 bytes");

    Report _report{};
    NimBLEServer* _server = nullptr;
    NimBLECharacteristic* _input = nullptr;
    NimBLECharacteristic* _battery = nullptr;
    bool _connected = false;
    bool _started = false;
};
