package com.airysdark.psxcore.update

import android.content.Context
import android.net.Uri
import android.util.Log
import com.airysdark.psxcore.ble.BleConnectionManager
import com.airysdark.psxcore.ble.ConnectionState
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

    fun selectFile(uri: Uri, name: String, size: Long) {
        _selectedFileUri.value = uri
        _selectedFileName.value = name
        _selectedFileSize.value = size
        _updateState.value = UpdateState.FILE_SELECTED
        _progress.value = 0f
    }

    suspend fun startUpdate() {
        val uri = _selectedFileUri.value ?: return
        val size = _selectedFileSize.value
        
        if (bleManager.connectionState.value != ConnectionState.READY) {
            _updateState.value = UpdateState.FAILED
            return
        }

        _updateState.value = UpdateState.PREPARING
        
        try {
            // 1. Send OTA_BEGIN
            val beginMsg = JSONObject().apply {
                put("type", "ota_begin")
                put("size", size)
            }.toString() + "\n"
            
            if (!bleManager.sendCommand(beginMsg)) {
                throw Exception("Failed to send OTA_BEGIN")
            }

            // 2. Wait for OTA_READY
            val chunkSize = withTimeoutOrNull(5000) {
                bleManager.otaReady.first()
            } ?: throw Exception("Timeout waiting for OTA_READY")

            _updateState.value = UpdateState.TRANSFERRING
            
            // 3. Send binary chunks
            context.contentResolver.openInputStream(uri)?.use { inputStream ->
                val buffer = ByteArray(chunkSize)
                var bytesSent = 0L
                var read: Int
                
                while (withContext(Dispatchers.IO) { inputStream.read(buffer) }.also { read = it } != -1) {
                    val chunk = if (read == chunkSize) buffer else buffer.copyOf(read)
                    if (!bleManager.sendBinaryChunk(chunk)) {
                        throw Exception("Failed to send chunk at $bytesSent")
                    }
                    bytesSent += read
                    _progress.value = bytesSent.toFloat() / size
                }
            } ?: throw Exception("Failed to open firmware file")

            // 4. Send OTA_END
            _updateState.value = UpdateState.VERIFYING
            val endMsg = JSONObject().apply {
                put("type", "ota_end")
            }.toString() + "\n"
            
            if (!bleManager.sendCommand(endMsg)) {
                throw Exception("Failed to send OTA_END")
            }

            // 5. Wait for Result
            val result = withTimeoutOrNull(30000) {
                bleManager.otaResult.first()
            } ?: throw Exception("Timeout waiting for OTA result")

            if (result.isSuccess) {
                _updateState.value = UpdateState.COMPLETE
            } else {
                throw result.exceptionOrNull() ?: Exception("Unknown OTA error")
            }

        } catch (e: Exception) {
            Log.e(tag, "OTA Update failed: ${e.message}")
            _updateState.value = UpdateState.FAILED
        }
    }
    
    fun reset() {
        _updateState.value = UpdateState.IDLE
        _progress.value = 0f
        _selectedFileUri.value = null
        _selectedFileName.value = null
        _selectedFileSize.value = 0L
    }
}
