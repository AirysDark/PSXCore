# ESP32-S3 PS2 Controller Wiring

## PS2 Cable Replacement

| PS2 Signal | ESP32-S3 |
|---|---|
| DATA | GPIO 4 |
| COMMAND | GPIO 5 |
| ATTENTION | GPIO 6 |
| CLOCK | GPIO 7 |
| ACK | GPIO 8 |
| 3.3V | 3V3 |
| GND | GND |

Not connected:

- Purple wire: 7V rumble power
- Gray wire: no connection

The ESP32-S3 will handle input reading and Bluetooth HID output.
