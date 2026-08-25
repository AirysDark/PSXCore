package com.airysdark.psxcore.model

data class BleDevice(
    val name: String?,
    val address: String,
    val rssi: Int = 0
) {
    val displayName: String get() = name ?: "Unknown Device"
}
