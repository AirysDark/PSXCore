# PSXCore ESP32-S3 Build

## Hardware
- ESP32-S3 Tiny
- Original PS2 controller PCB
- PS2 cable replaced by ESP32-S3

## Wiring
DATA -> GPIO4
COMMAND -> GPIO5
ATTENTION -> GPIO6
CLOCK -> GPIO7
ACK -> GPIO8

Power:
3.3V -> controller logic
GND -> controller ground

Do not connect:
- Purple 7V rumble wire
- Gray unused wire

## Firmware flow
PSX bus -> decoder -> controller state -> BLE HID
