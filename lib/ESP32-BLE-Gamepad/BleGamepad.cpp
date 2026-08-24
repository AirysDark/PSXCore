#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "sdkconfig.h"
#include "BleConnectionStatus.h"
#include "BleGamepad.h"
#include "NimBLELog.h"
#include "BleGamepadConfiguration.h"

#include <stdexcept>

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "BLEGamepad"
#else
#include "esp_log.h"
static const char *LOG_TAG = "BLEGamepad";
#endif

#define SERVICE_UUID_DEVICE_INFORMATION         "180A"
#define CHARACTERISTIC_UUID_MODEL_NUMBER        "2A24"
#define CHARACTERISTIC_UUID_SOFTWARE_REVISION   "2A28"
#define CHARACTERISTIC_UUID_SERIAL_NUMBER       "2A25"
#define CHARACTERISTIC_UUID_FIRMWARE_REVISION   "2A26"
#define CHARACTERISTIC_UUID_HARDWARE_REVISION   "2A27"
#define CHARACTERISTIC_UUID_BATTERY_POWER_STATE "2A1A"

// The remainder of this file is unchanged from the current repository version.
