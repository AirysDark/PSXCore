#pragma once

#include <stdint.h>

class DS4AuthI2C {
public:
    bool begin(int sda, int scl);
    bool sendChallenge(uint8_t tid, const uint8_t challenge[32], uint8_t response[42]);
};
