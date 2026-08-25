package com.airysdark.psxcore.ui

import android.Manifest
import android.app.Application
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.SystemClock
import android.util.Log
import android.widget.Toast
import androidx.core.content.ContextCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.airysdark.psxcore.ble.BleConnectionManager
import com.airysdark.psxcore.ble.BleScanner
import com.airysdark.psxcore.data.SettingsRepository
import com.airysdark.psxcore.model.BleDevice
import com.airysdark.psxcore.protocol.ProtocolConstants
import com.airysdark.psxcore.update.UpdateManager
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val tag = "MainViewModel"
    private val settingsRepository = SettingsRepository(application)

    private val bluetoothManager = application.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    val bleScanner = BleScanner(bluetoothManager.adapter)

    val bleConnectionManager = BleConnectionManager(application)
    val updateManager = UpdateManager(application, bleConnectionManager)

    val lastDeviceAddress: StateFlow<String?> = settingsRepository.lastDeviceAddress
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), null)

    val lastDeviceName: StateFlow<String?> = settingsRepository.lastDeviceName
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), null)

    val controllerInputState = bleConnectionManager.inputState
    val deviceInfo = bleConnectionManager.deviceInfo
    val deviceSettings = bleConnectionManager.deviceSettings
    val batteryLevel = bleConnectionManager.batteryLevel

    val isGamepadServiceReady = bleConnectionManager.isGamepadServiceReady
    val isCompanionServiceReady = bleConnectionManager.isCompanionServiceReady
    val isOtaReadyStatus = bleConnectionManager.isOtaReadyStatus

    private val commandLock = Any()
    private val lastCommandAt = mutableMapOf<String, Long>()

    private fun sendControlCommand(command: String, cooldownMs: Long): Boolean {
        val now = SystemClock.elapsedRealtime()
        synchronized(commandLock) {
            val previous = lastCommandAt[command]
            if (previous != null && now - previous < cooldownMs) {
                Log.d(tag, "[BLE] Duplicate command suppressed: $command (${now - previous}ms since previous)")
                return false
            }

            val queued = bleConnectionManager.sendCommand(command)
            if (queued) {
                lastCommandAt[command] = now
            } else {
                Log.w(tag, "[BLE] Command was not queued: $command")
            }
            return queued
        }
    }

    fun startScan(serviceUuid: java.util.UUID? = null, name: String? = null) {
        if (!hasScanPermission()) {
            Toast.makeText(getApplication(), "Bluetooth scan permission required", Toast.LENGTH_SHORT).show()
            return
        }
        bleScanner.startScan(serviceUuid, name)
    }

    fun stopScan() {
        bleScanner.stopScan()
    }

    private fun hasScanPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(getApplication(), Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(getApplication(), Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    fun connectToDevice(device: BleDevice) {
        if (!hasBluetoothPermission()) {
            Toast.makeText(getApplication(), "Bluetooth permission required", Toast.LENGTH_SHORT).show()
            return
        }
        bleConnectionManager.connect(device.address)
        viewModelScope.launch {
            settingsRepository.saveLastDevice(device.address, device.name)
        }
    }

    fun reconnect() {
        if (!hasBluetoothPermission()) {
            Toast.makeText(getApplication(), "Bluetooth permission required", Toast.LENGTH_SHORT).show()
            return
        }
        lastDeviceAddress.value?.let { address ->
            bleConnectionManager.connect(address)
        }
    }

    private fun hasBluetoothPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(getApplication(), Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
    }

    fun disconnect() {
        synchronized(commandLock) {
            lastCommandAt.clear()
        }
        bleConnectionManager.disconnect()
    }

    fun sendPing(): Boolean = sendControlCommand(ProtocolConstants.CMD_PING, 350)

    fun sendGetInfo(): Boolean = sendControlCommand(ProtocolConstants.CMD_INFO, 500)

    fun sendGetState(): Boolean = sendControlCommand(ProtocolConstants.CMD_GET_STATE, 350)

    fun sendGetSettings(): Boolean = sendControlCommand(ProtocolConstants.CMD_GET_SETTINGS, 500)

    fun setAnalogMode(): Boolean = sendControlCommand(ProtocolConstants.CMD_SET_ANALOG, 1000)

    fun requestOtaInfo(): Boolean = sendControlCommand(ProtocolConstants.CMD_OTA_INFO, 500)

    fun startOtaUpdate() {
        viewModelScope.launch {
            updateManager.startUpdate()
        }
    }

    fun onFileSelected(uri: Uri, name: String, size: Long) {
        val context = getApplication<Application>()
        var fileName = name
        var fileSize = size

        context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
            val sizeIndex = cursor.getColumnIndex(android.provider.OpenableColumns.SIZE)
            if (cursor.moveToFirst()) {
                if (nameIndex != -1) fileName = cursor.getString(nameIndex)
                if (sizeIndex != -1) fileSize = cursor.getLong(sizeIndex)
            }
        }

        viewModelScope.launch {
            updateManager.selectFile(uri, fileName, fileSize)
        }
    }
}
