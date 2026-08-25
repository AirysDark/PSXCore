package com.airysdark.psxcore.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.airysdark.psxcore.model.BleDevice
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class BleScanner(bluetoothAdapter: BluetoothAdapter?) {
    private val tag = "BleScanner"
    private val scanner = bluetoothAdapter?.bluetoothLeScanner
    
    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()
    
    private val _foundDevices = MutableStateFlow<List<BleDevice>>(emptyList())
    val foundDevices: StateFlow<List<BleDevice>> = _foundDevices.asStateFlow()
    
    private val handler = Handler(Looper.getMainLooper())
    
    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val bleDevice = BleDevice(device.name, device.address, result.rssi)
            
            val currentList = _foundDevices.value.toMutableList()
            val existingIndex = currentList.indexOfFirst { it.address == device.address }
            
            if (existingIndex != -1) {
                currentList[existingIndex] = bleDevice
            } else {
                currentList.add(bleDevice)
            }
            _foundDevices.value = currentList.sortedByDescending { it.rssi }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(tag, "Scan failed with error: $errorCode")
            _isScanning.value = false
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(timeoutMs: Long = 10000) {
        if (scanner == null || _isScanning.value) return
        
        Log.d(tag, "Starting BLE scan")
        _foundDevices.value = emptyList()
        _isScanning.value = true
        scanner.startScan(scanCallback)
        
        handler.postDelayed({
            if (_isScanning.value) {
                stopScan()
            }
        }, timeoutMs)
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!_isScanning.value) return
        
        Log.d(tag, "Stopping BLE scan")
        scanner?.stopScan(scanCallback)
        _isScanning.value = false
    }
}
