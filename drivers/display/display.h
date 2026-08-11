#pragma once

#include <Arduino.h>
#include "../spi/spi_bus.h"

class Display {
public:
    void begin();
    void reset();
    void command(uint8_t cmd);
    void data(uint8_t value);
    void clear(uint16_t color);

private:
    SPIBus spi;
    uint8_t cs;
    uint8_t dc;
    uint8_t rst;
};
