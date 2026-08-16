# PSXCore Control

Cross-platform client for PSXCore. The client is a Tauri 2 application so the same UI can target Windows and Android.

## Current client

- Console/status dashboard
- Controller monitor placeholder
- PSX GPIO pin mapping
- Pin sweep command UI
- Device settings
- Firmware/update status UI
- Responsive layout for phone screens

The UI is intentionally a client shell at this stage. Hardware transport commands are not faked: buttons record requests locally until the PSXCore transport protocol is implemented.

## Windows

```bash
cd client
npm install
npm run tauri build
```

For development:

```bash
npm run tauri dev
```

## Android

Install the Tauri Android prerequisites, then:

```bash
cd client
npm install
npm run tauri android init
npm run tauri android build
```

The Android target uses the same frontend and Tauri application shell.
