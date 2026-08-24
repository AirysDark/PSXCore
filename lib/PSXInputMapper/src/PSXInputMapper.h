#pragma once

#include <Arduino.h>
#include <PSXController.h>

/**
 * Normalized input state used by transport layers such as BLE HID.
 * Axes use -127..127. Hat uses HID values 0..7 and 8 for neutral.
 */
struct PSXInputState {
    bool connected = false;
    uint16_t buttons = 0;
    uint8_t hat = 8;

    int8_t leftX = 0;
    int8_t leftY = 0;
    int8_t rightX = 0;
    int8_t rightY = 0;

    uint8_t l2 = 0;
    uint8_t r2 = 0;
};

struct PSXInputConfig {
    uint8_t leftDeadZone = 12;
    uint8_t rightDeadZone = 12;
    bool invertLeftY = true;
    bool invertRightY = true;
    bool mapDpadToHat = true;
    bool removeDpadFromButtons = false;
    bool usePressureTriggers = true;
    bool digitalTriggersFullScale = true;
};

class PSXInputMapper {
public:
    explicit PSXInputMapper(const PSXInputConfig& config = PSXInputConfig{});

    PSXInputState map(const PSXControllerState& raw) const;
    void reset();

    void setConfig(const PSXInputConfig& config);
    const PSXInputConfig& config() const;

    void setLeftDeadZone(uint8_t deadZone);
    void setRightDeadZone(uint8_t deadZone);
    void setInvertLeftY(bool enabled);
    void setInvertRightY(bool enabled);

private:
    PSXInputConfig _config;

    static constexpr uint8_t HAT_NEUTRAL = 8;

    int8_t mapAxis(uint8_t value, uint8_t deadZone, bool invert) const;
    uint8_t mapHat(uint16_t buttons) const;
    uint16_t mapButtons(uint16_t buttons) const;
    uint8_t mapTrigger(bool pressed, uint8_t pressure) const;
};
