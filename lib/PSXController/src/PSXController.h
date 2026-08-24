#pragma once

#include <Arduino.h>

/**
 * Raw state read from a PlayStation / PS2 controller.
 * Buttons use true = pressed. Stick and pressure values use 0..255.
 */
struct PSXControllerState {
    bool connected = false;
    bool analogMode = false;
    uint16_t buttons = 0;

    uint8_t leftX = 128;
    uint8_t leftY = 128;
    uint8_t rightX = 128;
    uint8_t rightY = 128;

    uint8_t pressureUp = 0;
    uint8_t pressureRight = 0;
    uint8_t pressureDown = 0;
    uint8_t pressureLeft = 0;
    uint8_t pressureTriangle = 0;
    uint8_t pressureCircle = 0;
    uint8_t pressureCross = 0;
    uint8_t pressureSquare = 0;
    uint8_t pressureL1 = 0;
    uint8_t pressureR1 = 0;
};

enum class PSXButton : uint8_t {
    Select = 0,
    L3,
    R3,
    Start,
    Up,
    Right,
    Down,
    Left,
    L2,
    R2,
    L1,
    R1,
    Triangle,
    Circle,
    Cross,
    Square
};

struct PSXControllerPins {
    int8_t data = -1;
    int8_t command = -1;
    int8_t attention = -1;
    int8_t clock = -1;
};

class PSXController {
public:
    PSXController() = default;
    explicit PSXController(const PSXControllerPins& pins);

    bool begin();
    bool begin(const PSXControllerPins& pins);
    void end();

    bool update();
    const PSXControllerState& state() const;
    PSXControllerState read();

    bool connected() const;
    bool analogMode() const;
    bool pressed(PSXButton button) const;
    uint8_t pressure(PSXButton button) const;

    const PSXControllerPins& pins() const;

private:
    PSXControllerPins _pins{};
    PSXControllerState _state{};
    bool _started = false;

    void resetState();
    uint8_t transfer(uint8_t value);
    bool poll();
};
