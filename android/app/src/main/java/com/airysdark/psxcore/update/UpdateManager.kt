package com.airysdark.psxcore.update

import android.net.Uri
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

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

class UpdateManager {
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
    }

    fun startUpdate() {
        if (_updateState.value != UpdateState.FILE_SELECTED) return
        
        // This is where the actual protocol implementation would go.
        // For now, we just show that it's "not available until firmware protocol is connected".
        _updateState.value = UpdateState.FAILED
    }
    
    fun reset() {
        _updateState.value = UpdateState.IDLE
        _progress.value = 0f
        _selectedFileUri.value = null
        _selectedFileName.value = null
        _selectedFileSize.value = 0L
    }
}
