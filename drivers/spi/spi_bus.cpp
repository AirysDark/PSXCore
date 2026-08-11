#include "spi_bus.h"

static uint8_t _sck;
static uint8_t _mosi;

void SPIBus::begin(uint8_t sck, uint8_t mosi)
{
    _sck = sck;
    _mosi = mosi;
    pinMode(_sck, OUTPUT);
    pinMode(_mosi, OUTPUT);
}

void SPIBus::write(uint8_t data)
{
    for(int i = 7; i >= 0; i--)
    {
        digitalWrite(_sck, LOW);
        digitalWrite(_mosi, (data >> i) & 1);
        digitalWrite(_sck, HIGH);
    }
}

void SPIBus::writeBuffer(const uint8_t* data, uint32_t length)
{
    while(length--) write(*data++);
}
