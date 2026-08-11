# DS4 Auth Standalone Test Plan

The DS4 authentication module remains isolated from PSXCore.

Stages:

1. Verify I2C communication
- Detect authentication device
- Confirm addresses 0x60/0x61

2. Capture transaction
- Send known challenge
- Record 42 byte response

3. Validate protocol
- Confirm packet sizes
- Confirm timing

4. Integrate later
- Only after standalone module is proven.
