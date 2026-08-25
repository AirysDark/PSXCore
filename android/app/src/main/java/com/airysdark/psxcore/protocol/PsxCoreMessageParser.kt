package com.airysdark.psxcore.protocol

import com.airysdark.psxcore.model.ControllerInputState
import com.airysdark.psxcore.model.DeviceInfo
import com.airysdark.psxcore.model.DeviceSettings
import com.airysdark.psxcore.model.PsxButton
import com.airysdark.psxcore.model.PsxButtonMapping
import org.json.JSONObject

data class OtaStatus(
    val state: String,
    val receivedSize: Long,
    val expectedSize: Long,
    val progress: Float,
    val availableSpace: Long,
    val error: String? = null,
    val rebooting: Boolean = false
)

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

    fun parseOtaStatus(message: String): OtaStatus? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "ota") {
                OtaStatus(
                    state = json.optString("state", ""),
                    receivedSize = json.optLong("receivedSize", 0),
                    expectedSize = json.optLong("expectedSize", 0),
                    progress = json.optDouble("progress", 0.0).toFloat(),
                    availableSpace = json.optLong("availableSpace", 0),
                    error = json.optString("error", "").let { if (it.isEmpty()) null else it },
                    rebooting = json.optBoolean("rebooting", false)
                )
            } else null
        } catch (e: Exception) {
            null
        }
    }

    fun parseOtaReady(message: String): Int? {
        if (message == "OTA_READY") return 180
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "ota" && (json.optString("state") == "ready" || json.optString("state") == "receiving")) {
                json.optInt("chunk_size", 180)
            } else null
        } catch (e: Exception) {
            null
        }
    }

    fun parseOtaSuccess(message: String): Boolean {
        if (message == "OTA_SUCCESS") return true
        return try {
            val json = JSONObject(message)
            json.optString("type") == "ota" && json.optString("state") == "success"
        } catch (e: Exception) {
            false
        }
    }

    fun parseOtaError(message: String): String? {
        if (message.startsWith("OTA_ERROR:")) return message.substring(10)
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "ota" && json.optString("state") == "error") {
                json.optString("error", "Unknown OTA error")
            } else null
        } catch (e: Exception) {
            null
        }
    }
}
