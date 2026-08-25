package com.airysdark.psxcore.update

import android.content.Context
import android.net.Uri
import android.util.Log
import com.airysdark.psxcore.ble.BleConnectionManager
import com.airysdark.psxcore.ble.ConnectionState
import com.airysdark.psxcore.protocol.PsxCoreMessageParser
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONObject

enum class UpdateState {
    IDLE,
    FILE_SELECTED,
    CONNECTING,
    PREPARING,
    TRANSFERRING,
    VERIFYING,
    COMPLETE,
    FAILED
}

class UpdateManager(
    private val context: Context,
    private val bleManager: BleConnectionManager
) {
    private val tag = "UpdateManager"
    
    private val _updateState = MutableStateFlow(UpdateState.IDLE)
    val updateState: StateFlow<UpdateState> = _updateState.asStateFlow()
    
    private val _progress = MutableStateFlow(0f)
    val progress: StateFlow<Float> = _progress.asStateFlow()
    
    private val _selectedFileUri = MutableStateFlow<Uri?>(null)
    val selectedFileUri: StateFlow<Uri?> = _selectedFileUri.asStateFlow()
    
    private val _selectedFileName = MutableStateFlow<String?>(null)
    val selectedFileName: StateFlow<String?> = _selectedFileName.asStateFlow()

    private val _selectedFileSize = MutableStateFlow(0L)
    val selectedFileSize: StateFlow<Long> = _selectedFileSize.asStateFlow()

    private val _otaStatusText = MutableStateFlow("")
    val otaStatusText: StateFlow<String> = _otaStatusText.asStateFlow()

    fun selectFile(uri: Uri, name: String, size: Long) {
        _selectedFileUri.value = uri
        _selectedFileName.value = name
        _selectedFileSize.value = size
        _updateState.value = UpdateState.FILE_SELECTED
        _progress.value = 0f
        _otaStatusText.value = "File selected: $name"
    }

    suspend fun startUpdate() {
        val uri = _selectedFileUri.value ?: return
        val size = _selectedFileSize.value
        
        if (bleManager.connectionState.value != ConnectionState.READY) {
            _updateState.value = UpdateState.FAILED
            _otaStatusText.value = "Not connected to controller"
            return
        }

        _updateState.value = UpdateState.PREPARING
        _otaStatusText.value = "Starting OTA update..."
        
        try {
            // 1. Send OTA_BEGIN:<SIZE> to OTA_CONTROL
            val beginMsg = "OTA_BEGIN:$size"
            if (!bleManager.sendOtaControl(beginMsg)) {
                throw Exception("Failed to send OTA_BEGIN")
            }

            // 2. Wait for OTA_READY from OTA_STATUS (using the parser's logic in BleConnectionManager)
            _otaStatusText.value = "Waiting for controller to be ready..."
            val chunkSize = withTimeoutOrNull(10000) {
                bleManager.otaReady.first()
            } ?: throw Exception("Timeout waiting for OTA_READY")

            _updateState.value = UpdateState.TRANSFERRING
            _otaStatusText.value = "Transferring firmware..."
            
            // 3. Send binary chunks to OTA_DATA
            context.contentResolver.openInputStream(uri)?.use { inputStream ->
                val buffer = ByteArray(chunkSize)
                var bytesSent = 0L
                var read: Int
                
                while (withContext(Dispatchers.IO) { inputStream.read(buffer) }.also { read = it } != -1) {
                    // Check for errors during transfer
                    // (In a more advanced version, we would observe bleManager.otaResult for failures)
                    
                    val chunk = if (read == chunkSize) buffer else buffer.copyOf(read)
                    if (!bleManager.sendOtaData(chunk)) {
                        throw Exception("BLE write failure during transfer")
                    }
                    bytesSent += read
                    _progress.value = bytesSent.toFloat() / size
                    _otaStatusText.value = "Sent $bytesSent / $size bytes"
                    
                    // Throttle slightly to avoid overwhelming Android's BLE stack
                    kotlinx.coroutines.delay(10)
                }
            } ?: throw Exception("Failed to open firmware file")

            // 4. Send OTA_END to OTA_CONTROL
            _updateState.value = UpdateState.VERIFYING
            _otaStatusText.value = "Finalizing update..."
            if (!bleManager.sendOtaControl("OTA_END")) {
                throw Exception("Failed to send OTA_END")
            }

            // 5. Wait for Result (OTA_SUCCESS) from OTA_STATUS
            val result = withTimeoutOrNull(60000) {
                bleManager.otaResult.first()
            } ?: throw Exception("Timeout waiting for verification result")

            if (result.isSuccess) {
                _updateState.value = UpdateState.COMPLETE
                _otaStatusText.value = "Update successful! Controller is rebooting."
            } else {
                throw result.exceptionOrNull() ?: Exception("Unknown OTA error")
            }

        } catch (e: Exception) {
            Log.e(tag, "OTA Update failed: ${e.message}")
            _updateState.value = UpdateState.FAILED
            _otaStatusText.value = "Error: ${e.message}"
        }
    }
    
    fun reset() {
        _updateState.value = UpdateState.IDLE
        _progress.value = 0f
        _selectedFileUri.value = null
        _selectedFileName.value = null
        _selectedFileSize.value = 0L
        _otaStatusText.value = ""
    }
}
