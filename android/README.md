# PSXCore Android

Native Android companion application for configuring and updating PSXCore over Bluetooth Low Energy.

## Initial features

- Scan for PSXCore BLE devices
- Connect to the PSXCore configuration service
- Send `PING` and display `PONG`
- Request `INFO` and display firmware/device capabilities
- Foundation for controller configuration and BLE OTA updates

The firmware exposes the configuration channel through Nordic UART Service (NUS) on the same NimBLE server as the BLE HID gamepad.
