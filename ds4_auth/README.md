# DS4 Authentication Module

Reverse engineering notes and ESP32-S3 interface for DualShock 4 authentication.

Purpose:
- Document DS4 challenge-response authentication.
- Prepare PSXCore for DS4 authentication integration.
- Interface with external authentication hardware over I2C.

Current findings:
- Challenge size: 32 bytes
- Response size: 42 bytes
- I2C write address: 0x60
- I2C read address: 0x61
