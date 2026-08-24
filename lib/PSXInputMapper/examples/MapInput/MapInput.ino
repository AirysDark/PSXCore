#include <Arduino.h>
#include <PSXInputMapper.h>

void printState(const PSXInputState& s) {
    Serial.printf(
        "connected=%u buttons=%04X hat=%u LX=%d LY=%d RX=%d RY=%d L2=%u R2=%u\n",
        s.connected,
        s.buttons,
        s.hat,
        s.leftX, s.leftY,
        s.rightX, s.rightY,
        s.l2, s.r2
    );
}

void setup() {
    Serial.begin(115200);
    delay(500);

    PSXInputConfig config;
    config.leftDeadZone = 12;
    config.rightDeadZone = 12;
    config.invertLeftY = true;
    config.invertRightY = true;
    config.mapDpadToHat = true;
    config.removeDpadFromButtons = true;

    PSXInputMapper mapper(config);

    PSXControllerState raw{};
    raw.connected = true;
    raw.leftX = 255;
    raw.leftY = 0;
    raw.rightX = 128;
    raw.rightY = 255;
    raw.buttons = (1u << static_cast<uint8_t>(PSXButton::Up)) |
                  (1u << static_cast<uint8_t>(PSXButton::Right)) |
                  (1u << static_cast<uint8_t>(PSXButton::Cross)) |
                  (1u << static_cast<uint8_t>(PSXButton::L2));
    raw.pressureL2 = 180;

    printState(mapper.map(raw));
}

void loop() {
    delay(1000);
}
