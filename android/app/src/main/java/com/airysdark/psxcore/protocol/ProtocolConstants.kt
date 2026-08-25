package com.airysdark.psxcore.protocol

import java.util.UUID

object ProtocolConstants {
    /**
     * PSXCore Custom GATT BLE Service Contract (v6)
     */
    
    // Service UUID
    val PSXCORE_SERVICE_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450001")
    
    // 1. COMMAND - Android -> ESP32 (WRITE, WRITE_NO_RESPONSE)
    val PSX_COMMAND_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450002")
    
    // 2. RESPONSE - ESP32 -> Android (NOTIFY)
    val PSX_RESPONSE_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450003")
    
    // 3. CONTROLLER_STATE - ESP32 -> Android (NOTIFY)
    val PSX_CONTROLLER_STATE_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450004")
    
    // 4. OTA CONTROL - Android -> ESP32 (WRITE, WRITE_NO_RESPONSE)
    val PSX_OTA_CONTROL_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450005")
    
    // 5. OTA DATA - Android -> ESP32 (WRITE_NO_RESPONSE)
    val PSX_OTA_DATA_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450006")
    
    // 6. OTA STATUS - ESP32 -> Android (NOTIFY)
    val PSX_OTA_STATUS_UUID: UUID = UUID.fromString("7a4f0000-0000-4f50-5358-434f52450007")

    // Standard HID Service
    val HID_SERVICE_UUID: UUID = UUID.fromString("00001812-0000-1000-8000-00805f9b34fb")

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
