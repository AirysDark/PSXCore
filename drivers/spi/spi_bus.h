#pragma once

#include <Arduino.h>

class SPIBus {
public:
    void begin(uint8_t sck, uint8_t mosi);
    void write(uint8_t data);
    void writeBuffer(const uint8_t* data, uint32_t length);
};
