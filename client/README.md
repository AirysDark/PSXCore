# PSXCore Control

Windows desktop client shell for PSXCore, built with Tauri 2.

## Current client

- Console/status dashboard
- Controller monitor placeholder
- PSX GPIO pin mapping
- Pin sweep command UI
- Device settings
- Firmware/update status UI

The UI is intentionally a client shell at this stage. Hardware transport commands are not faked: buttons record requests locally until a desktop transport protocol is implemented.

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
