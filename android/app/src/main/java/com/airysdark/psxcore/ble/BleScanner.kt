package com.airysdark.psxcore.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import com.airysdark.psxcore.model.BleDevice
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID

class BleScanner(bluetoothAdapter: BluetoothAdapter?) {
    private val tag = "BleScanner"
    private val scanner = bluetoothAdapter?.bluetoothLeScanner
    
    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()
    
    private val _foundDevices = MutableStateFlow<List<BleDevice>>(emptyList())
    val foundDevices: StateFlow<List<BleDevice>> = _foundDevices.asStateFlow()
    
    private val handler = Handler(Looper.getMainLooper())
    
    // Scan configuration
    private var targetServiceUuid: UUID? = null
    private var targetName: String? = null

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val scanRecord = result.scanRecord
            val deviceName = device.name ?: scanRecord?.deviceName
            val serviceUuids = scanRecord?.serviceUuids?.map { it.uuid } ?: emptyList()
            
            // Detailed Logging
            Log.d(tag, "[SCAN] Found: Name=$deviceName, Address=${device.address}, RSSI=${result.rssi}")
            Log.d(tag, "[SCAN]   - Service UUIDs: $serviceUuids")
            
            // Match logic: Name OR Service UUID
            var isMatch = false
            
            if (targetName != null && deviceName?.contains(targetName!!, ignoreCase = true) == true) {
                Log.d(tag, "[SCAN]   - Match found by Name: $targetName")
                isMatch = true
            }
            
            if (!isMatch && targetServiceUuid != null && serviceUuids.contains(targetServiceUuid)) {
                Log.d(tag, "[SCAN]   - Match found by Service UUID: $targetServiceUuid")
                isMatch = true
            }
            
            // If no filters are provided, show everything
            if (targetName == null && targetServiceUuid == null) {
                isMatch = true
            }

            if (isMatch) {
                val bleDevice = BleDevice(deviceName, device.address, result.rssi)
                val currentList = _foundDevices.value.toMutableList()
                val existingIndex = currentList.indexOfFirst { it.address == device.address }
                
                if (existingIndex != -1) {
                    currentList[existingIndex] = bleDevice
                } else {
                    currentList.add(bleDevice)
                }
                _foundDevices.value = currentList.sortedByDescending { it.rssi }
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(tag, "Scan failed with error: $errorCode")
            _isScanning.value = false
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(serviceUuid: UUID? = null, name: String? = null, timeoutMs: Long = 10000) {
        if (scanner == null || _isScanning.value) return
        
        Log.d(tag, "Starting BLE scan (filter: Service=$serviceUuid, Name=$name)")
        
        targetServiceUuid = serviceUuid
        targetName = name
        
        _foundDevices.value = emptyList()
        _isScanning.value = true
        
        // We use a BROAD scan (no hardware filters) to ensure we see the device 
        // even if the service UUID isn't in the primary advertisement packet.
        // We do manual filtering in onScanResult.
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
            
        scanner.startScan(null, settings, scanCallback)
        
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
