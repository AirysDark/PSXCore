package com.airysdark.psxcore.protocol

import java.util.UUID

object ProtocolConstants {
    /**
     * PSXCore BLE GATT Contract
     */
    
    // Custom PSXCore / Nordic UART Service
    val PSX_SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    
    // RX Characteristic - Android -> PSXCore (WRITE)
    val PSX_RX_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    
    // TX Characteristic - PSXCore -> Android (NOTIFY)
    val PSX_TX_UUID: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    
    // Battery Service
    val BATTERY_SERVICE_UUID: UUID = UUID.fromString("0000180f-0000-1000-8000-00805f9b34fb")
    val BATTERY_LEVEL_UUID: UUID = UUID.fromString("00002a19-0000-1000-8000-00805f9b34fb")
    
    // Device Information Service
    val DEVICE_INFO_SERVICE_UUID: UUID = UUID.fromString("0000180a-0000-1000-8000-00805f9b34fb")
    val FIRMWARE_REVISION_UUID: UUID = UUID.fromString("00002a26-0000-1000-8000-00805f9b34fb")
    val HARDWARE_REVISION_UUID: UUID = UUID.fromString("00002a27-0000-1000-8000-00805f9b34fb")
    val MANUFACTURER_NAME_UUID: UUID = UUID.fromString("00002a29-0000-1000-8000-00805f9b34fb")
    
    // HID Service
    val HID_SERVICE_UUID: UUID = UUID.fromString("00001812-0000-1000-8000-00805f9b34fb")
    
    // Standard Bluetooth Descriptor for Notifications
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    
    // Commands
    const val CMD_PING = "PING\n"
    const val CMD_INFO = "INFO\n"
    const val CMD_GET_STATE = "GET_STATE\n"
    const val CMD_GET_SETTINGS = "GET_SETTINGS\n"
    const val CMD_SET_ANALOG = "SET_ANALOG\n"
    const val CMD_OTA_INFO = "OTA_INFO\n"
}
