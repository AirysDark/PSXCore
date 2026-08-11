#include "auth_i2c.h"

// Standalone module only.
// No PSXCore integration yet.
// This will later implement the DS4 authentication I2C transaction.

bool DS4AuthI2C::begin()
{
    return true;
}

bool DS4AuthI2C::sendChallenge(uint8_t tid, const uint8_t challenge[32], uint8_t response[42])
{
    (void)tid;
    (void)challenge;
    (void)response;

    // TODO:
    // Write:
    // 0x60 + command + TID + 32 byte challenge
    // Read:
    // 42 byte authentication response

    return false;
}
