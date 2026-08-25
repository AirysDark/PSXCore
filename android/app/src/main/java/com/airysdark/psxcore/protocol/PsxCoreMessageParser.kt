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

/**
 * Decoder for the shared PSXCore GATT v7 wire contract.
 *
 * The decoder deliberately accepts both the original Android field names and
 * the fields emitted by the current ESP32 firmware. This keeps the companion
 * compatible while firmware and Android are updated independently.
 */
class PsxCoreMessageParser {
    fun parseState(message: String, currentCount: Long, currentAnalogMode: Boolean): ControllerInputState? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") != "state") return null

            val buttons = json.optInt("buttons", 0)
            val isAnalog = when {
                json.has("analog") -> json.optBoolean("analog", currentAnalogMode)
                json.has("analogMode") -> json.optBoolean("analogMode", currentAnalogMode)
                json.has("analog_mode") -> json.optBoolean("analog_mode", currentAnalogMode)
                json.has("mode") -> json.optString("mode").equals("ANALOG", ignoreCase = true)
                else -> currentAnalogMode
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
        } catch (_: Exception) {
            null
        }
    }

    fun parseDeviceInfo(message: String): DeviceInfo? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") != "info") return null

            val otaSupported = when {
                json.has("otaSupported") -> json.optBoolean("otaSupported", false)
                json.has("ota") && json.opt("ota") is Boolean -> json.optBoolean("ota", false)
                json.has("ota") -> json.optString("ota").isNotBlank() &&
                    !json.optString("ota").equals("false", ignoreCase = true)
                else -> false
            }

            DeviceInfo(
                firmwareVersion = json.optString("version", "Unknown"),
                hardwareRevision = json.optString(
                    "hardware",
                    if (json.optString("transport") == "shared-gatt") "ESP32-S3" else "Unknown"
                ),
                deviceName = json.optString("device", json.optString("name", "PSXCore")),
                buildDate = json.optString("build", "Unknown"),
                protocolVersion = json.optInt("protocol", 0),
                otaSupport = otaSupported
            )
        } catch (_: Exception) {
            null
        }
    }

    fun parseDeviceSettings(message: String): DeviceSettings? {
        return try {
            val json = JSONObject(message)
            if (json.optString("type") != "settings") return null

            val sleep = when {
                json.has("sleep") -> json.optInt("sleep", 5)
                json.has("sleepMinutes") -> json.optInt("sleepMinutes", 5)
                else -> 5
            }

            DeviceSettings(
                controllerName = json.optString("name", "PSXCore Controller"),
                sleepTimeoutMinutes = sleep,
                analogModeBehavior = when {
                    json.has("analog_behavior") -> json.optInt("analog_behavior", 0)
                    json.optBoolean("analogControl", false) -> 1
                    else -> 0
                },
                inactivityTimeoutMinutes = json.optInt("inactivity", sleep),
                autoReconnect = json.optBoolean("auto_reconnect", true)
            )
        } catch (_: Exception) {
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
        } catch (_: Exception) {
            null
        }
    }

    fun parseOtaReady(message: String): Int? {
        if (message == "OTA_READY") return 180
        return try {
            val json = JSONObject(message)
            if (json.optString("type") == "ota" &&
                (json.optString("state") == "ready" || json.optString("state") == "receiving")
            ) {
                json.optInt("chunk_size", 180)
            } else null
        } catch (_: Exception) {
            null
        }
    }

    fun parseOtaSuccess(message: String): Boolean {
        if (message == "OTA_SUCCESS") return true
        return try {
            val json = JSONObject(message)
            json.optString("type") == "ota" && json.optString("state") == "success"
        } catch (_: Exception) {
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
        } catch (_: Exception) {
            null
        }
    }
}
