package com.airysdark.psxcore.model

data class DeviceSettings(
    val controllerName: String = "PSXCore Controller",
    val sleepTimeoutMinutes: Int = 5,
    val analogModeBehavior: Int = 0, // 0: Manual, 1: Auto-On, 2: Persistent
    val inactivityTimeoutMinutes: Int = 5,
    val autoReconnect: Boolean = true
)
