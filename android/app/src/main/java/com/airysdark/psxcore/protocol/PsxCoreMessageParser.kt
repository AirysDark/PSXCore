package com.airysdark.psxcore.protocol

import com.airysdark.psxcore.model.ControllerInputState
import com.airysdark.psxcore.model.DeviceInfo
import com.airysdark.psxcore.model.DeviceSettings
import com.airysdark.psxcore.model.PsxButton
import com.airysdark.psxcore.model.PsxButtonMapping
import org.json.JSONObject

class PsxCoreMessageParser {
    fun parseState(message: String, currentCount: Long): ControllerInputState? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "state") {
                val buttons = json.optInt("buttons", 0)
                ControllerInputState(
                    dpadUp = PsxButtonMapping.isPressed(buttons, PsxButton.UP),
                    dpadDown = PsxButtonMapping.isPressed(buttons, PsxButton.DOWN),
                    dpadLeft = PsxButtonMapping.isPressed(buttons, PsxButton.LEFT),
                    dpadRight = PsxButtonMapping.isPressed(buttons, PsxButton.RIGHT),
                    triangle = PsxButtonMapping.isPressed(buttons, PsxButton.TRIANGLE),
                    circle = PsxButtonMapping.isPressed(buttons, PsxButton.CIRCLE),
                    cross = PsxButtonMapping.isPressed(buttons, PsxButton.CROSS),
                    square = PsxButtonMapping.isPressed(buttons, PsxButton.SQUARE),
                    l1 = PsxButtonMapping.isPressed(buttons, PsxButton.L1),
                    l2 = PsxButtonMapping.isPressed(buttons, PsxButton.L2),
                    l3 = PsxButtonMapping.isPressed(buttons, PsxButton.L3),
                    r1 = PsxButtonMapping.isPressed(buttons, PsxButton.R1),
                    r2 = PsxButtonMapping.isPressed(buttons, PsxButton.R2),
                    r3 = PsxButtonMapping.isPressed(buttons, PsxButton.R3),
                    start = PsxButtonMapping.isPressed(buttons, PsxButton.START),
                    select = PsxButtonMapping.isPressed(buttons, PsxButton.SELECT),
                    analogButton = PsxButtonMapping.isPressed(buttons, PsxButton.ANALOG),
                    analogMode = json.optBoolean("analog", false),
                    leftStickX = json.optInt("lx", 128),
                    leftStickY = json.optInt("ly", 128),
                    rightStickX = json.optInt("rx", 128),
                    rightStickY = json.optInt("ry", 128),
                    lastInputTimestamp = System.currentTimeMillis(),
                    packetCount = currentCount
                )
            } else null
        } catch (e: Exception) {
            null
        }
    }

    fun parseDeviceInfo(message: String): DeviceInfo? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "info") {
                DeviceInfo(
                    firmwareVersion = json.optString("version", "Unknown"),
                    hardwareRevision = json.optString("hardware", "Unknown"),
                    deviceName = json.optString("device", json.optString("name", "PSXCore")),
                    buildDate = json.optString("build", "Unknown"),
                    otaSupport = json.optBoolean("ota", false)
                )
            } else null
        } catch (e: Exception) {
            null
        }
    }

    fun parseDeviceSettings(message: String): DeviceSettings? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "settings") {
                DeviceSettings(
                    controllerName = json.optString("name", "PSXCore Controller"),
                    sleepTimeoutMinutes = json.optInt("sleep", 5),
                    analogModeBehavior = json.optInt("analog_behavior", 0),
                    inactivityTimeoutMinutes = json.optInt("inactivity", 5),
                    autoReconnect = json.optBoolean("auto_reconnect", true)
                )
            } else null
        } catch (e: Exception) {
            null
        }
    }
}
