package com.airysdark.psxcore.update

data class FirmwareTransferState(
    val bytesTransferred: Long = 0,
    val totalBytes: Long = 0,
    val isTransferring: Boolean = false,
    val error: String? = null
) {
    val progress: Float get() = if (totalBytes > 0) bytesTransferred.toFloat() / totalBytes else 0f
}
