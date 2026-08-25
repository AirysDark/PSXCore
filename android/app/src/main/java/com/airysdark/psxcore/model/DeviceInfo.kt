package com.airysdark.psxcore.model

data class DeviceInfo(
    val firmwareVersion: String = "Unknown",
    val hardwareRevision: String = "Unknown",
    val deviceName: String = "PSXCore",
    val buildDate: String = "Unknown",
    val otaSupport: Boolean = false
)
