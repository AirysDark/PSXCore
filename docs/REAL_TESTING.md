# PSXCore Real Hardware Test

## Wiring
- Connect PS2 controller wires to ESP32-S3 according to pins.h.
- Do not connect the purple 7V rumble wire.
- Leave the grey no-connect wire isolated.

## Test sequence
1. Flash with PlatformIO.
2. Open serial monitor.
3. Power the ESP32-S3.
4. Connect the Bluetooth gamepad named PSXCore ESP32-S3.
5. Verify:
   - D-pad input
   - face buttons
   - shoulder buttons
   - analog sticks

## Debug order
If no input:
1. Check ATTENTION, CLOCK, COMMAND, DATA wiring.
2. Verify 3.3V logic compatibility.
3. Confirm controller packet response.
4. Test another PS2 controller PCB if needed.
