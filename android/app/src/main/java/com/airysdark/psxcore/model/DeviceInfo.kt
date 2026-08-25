package com.airysdark.psxcore.model

data class DeviceInfo(
    val firmwareVersion: String = "Unknown",
    val hardwareRevision: String = "Unknown",
    val deviceName: String = "PSXCore",
    val buildDate: String = "Unknown",
    val protocolVersion: Int = 0,
    val otaSupport: Boolean = false
)
