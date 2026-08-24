#include <Arduino.h>
#include <PSXController.h>

// Replace these with the PS2 controller pins for the active hardware target.
static const PSXControllerPins PINS = {
    .data = 5,
    .command = 6,
    .attention = 7,
    .clock = 8
};

PSXController controller(PINS);

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("PSXController library test");
    if (!controller.begin()) {
        Serial.println("Controller not detected at startup; retrying in loop.");
    }
}

void loop() {
    if (!controller.update()) {
        Serial.println("Controller disconnected");
        delay(500);
        return;
    }

    const PSXControllerState& s = controller.state();

    Serial.printf(
        "mode=%s buttons=%04X LX=%u LY=%u RX=%u RY=%u Cross=%u L2=%u R2=%u\n",
        s.analogMode ? "analog" : "digital",
        s.buttons,
        s.leftX, s.leftY,
        s.rightX, s.rightY,
        s.pressureCross,
        s.pressureL2,
        s.pressureR2
    );

    delay(50);
}
