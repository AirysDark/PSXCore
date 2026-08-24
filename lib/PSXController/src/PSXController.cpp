#include "PSXController.h"

PSXController::PSXController(const PSXControllerPins& pins) : _pins(pins) {}

bool PSXController::begin(const PSXControllerPins& pins) {
    _pins = pins;
    return begin();
}

bool PSXController::begin() {
    if (_pins.data < 0 || _pins.command < 0 || _pins.attention < 0 || _pins.clock < 0) {
        return false;
    }

    pinMode(_pins.data, INPUT_PULLUP);
    pinMode(_pins.command, OUTPUT);
    pinMode(_pins.attention, OUTPUT);
    pinMode(_pins.clock, OUTPUT);

    digitalWrite(_pins.command, HIGH);
    digitalWrite(_pins.attention, HIGH);
    digitalWrite(_pins.clock, HIGH);

    resetState();
    _started = true;
    return update();
}

void PSXController::end() {
    _started = false;
    resetState();
}

bool PSXController::update() {
    if (!_started) {
        return false;
    }
    return poll();
}

const PSXControllerState& PSXController::state() const {
    return _state;
}

PSXControllerState PSXController::read() {
    update();
    return _state;
}

bool PSXController::connected() const {
    return _state.connected;
}

bool PSXController::analogMode() const {
    return _state.analogMode;
}

bool PSXController::pressed(PSXButton button) const {
    return (_state.buttons & (1u << static_cast<uint8_t>(button))) != 0;
}

uint8_t PSXController::pressure(PSXButton button) const {
    switch (button) {
        case PSXButton::Up: return _state.pressureUp;
        case PSXButton::Right: return _state.pressureRight;
        case PSXButton::Down: return _state.pressureDown;
        case PSXButton::Left: return _state.pressureLeft;
        case PSXButton::Triangle: return _state.pressureTriangle;
        case PSXButton::Circle: return _state.pressureCircle;
        case PSXButton::Cross: return _state.pressureCross;
        case PSXButton::Square: return _state.pressureSquare;
        case PSXButton::L1: return _state.pressureL1;
        case PSXButton::R1: return _state.pressureR1;
        default: return 0;
    }
}

const PSXControllerPins& PSXController::pins() const {
    return _pins;
}

void PSXController::resetState() {
    _state = PSXControllerState{};
}

uint8_t PSXController::transfer(uint8_t value) {
    uint8_t result = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        digitalWrite(_pins.command, (value & (1u << bit)) ? HIGH : LOW);
        delayMicroseconds(2);
        digitalWrite(_pins.clock, LOW);
        delayMicroseconds(4);
        if (digitalRead(_pins.data)) {
            result |= (1u << bit);
        }
        digitalWrite(_pins.clock, HIGH);
        delayMicroseconds(4);
    }
    return result;
}

bool PSXController::poll() {
    digitalWrite(_pins.attention, LOW);
    delayMicroseconds(10);

    const uint8_t response0 = transfer(0x01);
    const uint8_t response1 = transfer(0x42);
    transfer(0x00);

    if (response0 != 0xFF || response1 == 0xFF || response1 == 0x00) {
        digitalWrite(_pins.attention, HIGH);
        _state.connected = false;
        return false;
    }

    const uint8_t buttonLow = transfer(0x00);
    const uint8_t buttonHigh = transfer(0x00);

    _state.buttons = static_cast<uint16_t>(~(buttonLow | (static_cast<uint16_t>(buttonHigh) << 8)));
    _state.connected = true;
    _state.analogMode = response1 == 0x73 || response1 == 0x79;

    if (_state.analogMode) {
        _state.rightX = transfer(0x00);
        _state.rightY = transfer(0x00);
        _state.leftX = transfer(0x00);
        _state.leftY = transfer(0x00);
    }

    // DualShock 2 pressure mode (0x79) exposes additional pressure bytes.
    if (response1 == 0x79) {
        transfer(0x00);
        transfer(0x00);
        _state.pressureUp = transfer(0x00);
        _state.pressureRight = transfer(0x00);
        _state.pressureDown = transfer(0x00);
        _state.pressureLeft = transfer(0x00);
        _state.pressureL2 = transfer(0x00);
        _state.pressureR2 = transfer(0x00);
        _state.pressureL1 = transfer(0x00);
        _state.pressureR1 = transfer(0x00);
        _state.pressureTriangle = transfer(0x00);
        _state.pressureCircle = transfer(0x00);
        _state.pressureCross = transfer(0x00);
        _state.pressureSquare = transfer(0x00);
    }

    digitalWrite(_pins.attention, HIGH);
    return true;
}
