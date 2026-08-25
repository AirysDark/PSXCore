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
    fun parseState(message: String, currentCount: Long, currentAnalogMode: Boolean): ControllerInputState? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "state") {
                val buttons = json.optInt("buttons", 0)
                
                // Robust Analog Mode detection (checking all common field name variations):
                val analogField = if (json.has("analog")) json.opt("analog") else null
                val analogModeField = if (json.has("analogMode")) json.opt("analogMode") else null
                val analog_modeField = if (json.has("analog_mode")) json.opt("analog_mode") else null
                val modeField = json.optString("mode", "")
                
                // Only update if one of the fields is actually present in the JSON.
                // Otherwise, preserve the current state to avoid flickering to false.
                val hasAnalogField = json.has("analog") || json.has("analogMode") || 
                                     json.has("analog_mode") || json.has("mode")
                
                val isAnalog = if (hasAnalogField) {
                    when {
                        analogField is Boolean -> analogField
                        analogField is Int -> analogField != 0
                        analogModeField is Boolean -> analogModeField
                        analogModeField is Int -> analogModeField != 0
                        analog_modeField is Boolean -> analog_modeField
                        analog_modeField is Int -> analog_modeField != 0
                        modeField.equals("ANALOG", ignoreCase = true) -> true
                        modeField.equals("DIGITAL", ignoreCase = true) -> false
                        else -> json.optBoolean("analog", json.optBoolean("analogMode", json.optBoolean("analog_mode", currentAnalogMode)))
                    }
                } else {
                    currentAnalogMode
                }

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
                    analogMode = isAnalog,
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
                    protocolVersion = json.optInt("protocol", 0),
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
