#include "display.h"

void Display::begin()
{
    spi.begin(12, 13);

    cs = 10;
    dc = 9;
    rst = 11;

    pinMode(cs, OUTPUT);
    pinMode(dc, OUTPUT);
    pinMode(rst, OUTPUT);

    reset();
}

void Display::reset()
{
    digitalWrite(rst, LOW);
    delay(20);
    digitalWrite(rst, HIGH);
    delay(120);
}

void Display::command(uint8_t cmd)
{
    digitalWrite(dc, LOW);
    digitalWrite(cs, LOW);
    spi.write(cmd);
    digitalWrite(cs, HIGH);
}

void Display::data(uint8_t value)
{
    digitalWrite(dc, HIGH);
    digitalWrite(cs, LOW);
    spi.write(value);
    digitalWrite(cs, HIGH);
}

void Display::clear(uint16_t color)
{
}
