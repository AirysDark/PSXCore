package com.airysdark.psxcore.protocol

import com.airysdark.psxcore.model.ControllerInputState

/**
 * Interface for parsing PSXCore firmware BLE input packets.
 */
interface PsxCoreInputParser {
    /**
     * Parses the raw BLE byte array into a ControllerInputState.
     * Returns null if the packet is invalid or not an input report.
     */
    fun parse(data: ByteArray, currentCount: Long): ControllerInputState?
}

/**
 * Placeholder implementation until the firmware protocol is finalized.
 */
class DefaultPsxCoreInputParser : PsxCoreInputParser {
    override fun parse(data: ByteArray, currentCount: Long): ControllerInputState? {
        // TODO: Implement actual protocol parsing based on firmware specifications.
        // For now, this returns null to avoid inventing a fake protocol.
        return null
    }
}
