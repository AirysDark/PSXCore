#pragma once

#define DS4_AUTH_I2C_WRITE 0x60
#define DS4_AUTH_I2C_READ  0x61

#define DS4_AUTH_CHALLENGE_SIZE 32
#define DS4_AUTH_HASH_SIZE 20
#define DS4_AUTH_RESPONSE_SIZE 42

typedef struct {
    uint8_t command;
    uint8_t tid;
    uint8_t challenge[DS4_AUTH_CHALLENGE_SIZE];
} DS4AuthRequest;

typedef struct {
    uint8_t header[2];
    uint8_t hash1[DS4_AUTH_HASH_SIZE];
    uint8_t hash2[DS4_AUTH_HASH_SIZE];
} DS4AuthResponse;
