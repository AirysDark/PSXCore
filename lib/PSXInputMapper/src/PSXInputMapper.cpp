#include "PSXInputMapper.h"

PSXInputMapper::PSXInputMapper(const PSXInputConfig& config) : _config(config) {}

PSXInputState PSXInputMapper::map(const PSXControllerState& raw) const {
    PSXInputState out{};
    out.connected = raw.connected;

    if (!raw.connected) {
        return out;
    }

    out.buttons = mapButtons(raw.buttons);
    out.hat = _config.mapDpadToHat ? mapHat(raw.buttons) : HAT_NEUTRAL;

    out.leftX = mapAxis(raw.leftX, _config.leftDeadZone, false);
    out.leftY = mapAxis(raw.leftY, _config.leftDeadZone, _config.invertLeftY);
    out.rightX = mapAxis(raw.rightX, _config.rightDeadZone, false);
    out.rightY = mapAxis(raw.rightY, _config.rightDeadZone, _config.invertRightY);

    const bool l2Pressed = (raw.buttons & (1u << static_cast<uint8_t>(PSXButton::L2))) != 0;
    const bool r2Pressed = (raw.buttons & (1u << static_cast<uint8_t>(PSXButton::R2))) != 0;

    const uint8_t l2Pressure = _config.usePressureTriggers ? raw.pressureL2 : 0;
    const uint8_t r2Pressure = _config.usePressureTriggers ? raw.pressureR2 : 0;

    out.l2 = mapTrigger(l2Pressed, l2Pressure);
    out.r2 = mapTrigger(r2Pressed, r2Pressure);
    return out;
}

void PSXInputMapper::reset() {}

void PSXInputMapper::setConfig(const PSXInputConfig& config) {
    _config = config;
}

const PSXInputConfig& PSXInputMapper::config() const {
    return _config;
}

void PSXInputMapper::setLeftDeadZone(uint8_t deadZone) {
    _config.leftDeadZone = deadZone > 127 ? 127 : deadZone;
}

void PSXInputMapper::setRightDeadZone(uint8_t deadZone) {
    _config.rightDeadZone = deadZone > 127 ? 127 : deadZone;
}

void PSXInputMapper::setInvertLeftY(bool enabled) {
    _config.invertLeftY = enabled;
}

void PSXInputMapper::setInvertRightY(bool enabled) {
    _config.invertRightY = enabled;
}

int8_t PSXInputMapper::mapAxis(uint8_t value, uint8_t deadZone, bool invert) const {
    int16_t centered = static_cast<int16_t>(value) - 128;
    deadZone = deadZone > 127 ? 127 : deadZone;

    if (abs(centered) <= deadZone) {
        return 0;
    }

    const int16_t sign = centered < 0 ? -1 : 1;
    const int16_t magnitude = abs(centered) - deadZone;
    const int16_t range = 128 - deadZone;
    int16_t mapped = sign * ((magnitude * 127 + (range / 2)) / range);

    if (mapped > 127) mapped = 127;
    if (mapped < -127) mapped = -127;
    if (invert) mapped = -mapped;

    return static_cast<int8_t>(mapped);
}

uint8_t PSXInputMapper::mapHat(uint16_t buttons) const {
    const bool up = (buttons & (1u << static_cast<uint8_t>(PSXButton::Up))) != 0;
    const bool right = (buttons & (1u << static_cast<uint8_t>(PSXButton::Right))) != 0;
    const bool down = (buttons & (1u << static_cast<uint8_t>(PSXButton::Down))) != 0;
    const bool left = (buttons & (1u << static_cast<uint8_t>(PSXButton::Left))) != 0;

    if (up && down) return HAT_NEUTRAL;
    if (left && right) return HAT_NEUTRAL;

    if (up && right) return 1;
    if (right && down) return 3;
    if (down && left) return 5;
    if (left && up) return 7;
    if (up) return 0;
    if (right) return 2;
    if (down) return 4;
    if (left) return 6;

    return HAT_NEUTRAL;
}

uint16_t PSXInputMapper::mapButtons(uint16_t buttons) const {
    if (!_config.removeDpadFromButtons) {
        return buttons;
    }

    constexpr uint16_t DPAD_MASK =
        (1u << static_cast<uint8_t>(PSXButton::Up)) |
        (1u << static_cast<uint8_t>(PSXButton::Right)) |
        (1u << static_cast<uint8_t>(PSXButton::Down)) |
        (1u << static_cast<uint8_t>(PSXButton::Left));

    return buttons & ~DPAD_MASK;
}

uint8_t PSXInputMapper::mapTrigger(bool pressed, uint8_t pressure) const {
    if (_config.usePressureTriggers && pressure != 0) {
        return pressure;
    }

    if (pressed && _config.digitalTriggersFullScale) {
        return 255;
    }

    return 0;
}
