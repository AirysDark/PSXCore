package com.airysdark.psxcore.protocol

import java.util.UUID

object ProtocolConstants {
    /**
     * PSXCore Custom GATT BLE Service Contract
     */
    
    // Service UUID
    val PSXCORE_SERVICE_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0000")
    
    // 1. COMMAND - Android -> ESP32 (WRITE, WRITE_NO_RESPONSE)
    val PSX_COMMAND_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0001")
    
    // 2. RESPONSE - ESP32 -> Android (NOTIFY)
    val PSX_RESPONSE_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0002")
    
    // 3. STATE - ESP32 -> Android (NOTIFY)
    val PSX_STATE_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0003")
    
    // 4. OTA CONTROL - Android -> ESP32 (WRITE, WRITE_NO_RESPONSE)
    val PSX_OTA_CONTROL_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0004")
    
    // 5. OTA DATA - Android -> ESP32 (WRITE_NO_RESPONSE)
    val PSX_OTA_DATA_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0005")
    
    // 6. OTA STATUS - ESP32 -> Android (NOTIFY)
    val PSX_OTA_STATUS_UUID: UUID = UUID.fromString("12345678-1234-5678-1234-56789abc0006")

    // Standard Bluetooth Descriptor for Notifications
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    
    // Commands
    const val CMD_PING = "PING"
    const val CMD_INFO = "INFO"
    const val CMD_GET_STATE = "GET_STATE"
    const val CMD_GET_SETTINGS = "GET_SETTINGS"
    const val CMD_SET_ANALOG = "SET_ANALOG"
    const val CMD_OTA_BEGIN = "OTA_BEGIN"
    const val CMD_OTA_END = "OTA_END"
    const val CMD_OTA_RESET = "OTA_RESET"
    const val CMD_OTA_CANCEL = "OTA_CANCEL"
    const val CMD_OTA_INFO = "OTA_INFO"
}
