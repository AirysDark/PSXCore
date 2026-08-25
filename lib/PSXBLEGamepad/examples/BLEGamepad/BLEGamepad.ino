#include <Arduino.h>
#include <PSXBLEGamepad.h>

PSXBLEGamepad gamepad;
PSXInputState input{};

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!gamepad.begin("PSXCore BLE Test")) {
        Serial.println("BLE HID startup failed");
        return;
    }

    input.connected = true;
    input.hat = 8;
    input.leftX = 0;
    input.leftY = 0;
    input.rightX = 0;
    input.rightY = 0;

    gamepad.setBatteryLevel(100);
    Serial.println("BLE gamepad advertising");
}

void loop() {
    static uint32_t lastUpdate = 0;
    static bool pressed = false;

    if (millis() - lastUpdate >= 1000) {
        lastUpdate = millis();
        pressed = !pressed;

        input.buttons = pressed
            ? (1u << static_cast<uint8_t>(PSXButton::Cross))
            : 0;

        input.leftX = pressed ? 64 : 0;
        input.hat = pressed ? 0 : 8;

        if (gamepad.connected()) {
            gamepad.send(input);
        }
    }
}
