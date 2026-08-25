package com.airysdark.psxcore.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.airysdark.psxcore.model.ControllerInputState
import com.airysdark.psxcore.model.DeviceInfo
import com.airysdark.psxcore.model.DeviceSettings
import com.airysdark.psxcore.protocol.ProtocolConstants
import com.airysdark.psxcore.protocol.PsxCoreMessageParser
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withTimeoutOrNull
import java.nio.charset.StandardCharsets

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    DISCOVERING_SERVICES,
    ENABLING_NOTIFICATIONS,
    READY,
    ERROR
}

class BleConnectionManager(private val context: Context) {
    private val tag = "BleConnectionManager"
    private var bluetoothGatt: BluetoothGatt? = null
    
    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()
    
    private val _connectedDeviceName = MutableStateFlow<String?>(null)
    val connectedDeviceName: StateFlow<String?> = _connectedDeviceName.asStateFlow()

    private val _receivedData = MutableStateFlow("")
    val receivedData: StateFlow<String> = _receivedData.asStateFlow()

    private val _inputState = MutableStateFlow(ControllerInputState())
    val inputState: StateFlow<ControllerInputState> = _inputState.asStateFlow()

    private val _deviceInfo = MutableStateFlow(DeviceInfo())
    val deviceInfo: StateFlow<DeviceInfo> = _deviceInfo.asStateFlow()

    private val _deviceSettings = MutableStateFlow(DeviceSettings())
    val deviceSettings: StateFlow<DeviceSettings> = _deviceSettings.asStateFlow()

    private val _batteryLevel = MutableStateFlow<Int?>(null)
    val batteryLevel: StateFlow<Int?> = _batteryLevel.asStateFlow()

    private val _otaReady = MutableSharedFlow<Int>(extraBufferCapacity = 1)
    val otaReady = _otaReady.asSharedFlow()

    private val _otaResult = MutableSharedFlow<Result<Unit>>(extraBufferCapacity = 1)
    val otaResult = _otaResult.asSharedFlow()

    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var batteryCharacteristic: BluetoothGattCharacteristic? = null

    private var pendingWrite: CompletableDeferred<Int>? = null

    private val parser = PsxCoreMessageParser()
    private var messageBuffer = StringBuilder()

    private var isConnecting = false
    private val handler = Handler(Looper.getMainLooper())
    private val timeoutRunnable = Runnable {
        if (isConnecting) {
            Log.e(tag, "[BLE] Connection timeout")
            handleError()
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            val address = gatt.device.address
            Log.d(tag, "[BLE] onConnectionStateChange: address=$address, status=$status, newState=$newState")

            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "[BLE] GATT error: $status")
                handleError()
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.d(tag, "[BLE] Connected to GATT server")
                    _connectionState.value = ConnectionState.DISCOVERING_SERVICES
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.d(tag, "[BLE] Disconnected from GATT server")
                    handleDisconnect()
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Services discovered")
                _connectionState.value = ConnectionState.ENABLING_NOTIFICATIONS
                setupCharacteristics(gatt)
            } else {
                Log.e(tag, "[BLE] Service discovery failed: $status")
                handleError()
            }
        }

        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            // On some Android versions, both onCharacteristicChanged overloads are called.
            // We use a flag or only handle the modern one if we're on Tiramisu+.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                processData(characteristic.value ?: return, characteristic)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            processData(value, characteristic)
        }

        private fun processData(data: ByteArray, characteristic: BluetoothGattCharacteristic) {
            when (characteristic.uuid) {
                ProtocolConstants.PSX_TX_UUID -> {
                    val chunk = String(data, StandardCharsets.UTF_8)
                    
                    synchronized(messageBuffer) {
                        messageBuffer.append(chunk)
                        
                        var newlineIndex: Int
                        while (messageBuffer.indexOf("\n").also { newlineIndex = it } >= 0) {
                            val message = messageBuffer.substring(0, newlineIndex).trim()
                            messageBuffer.delete(0, newlineIndex + 1)
                            
                            if (message.isNotEmpty()) {
                                Log.d(tag, "[BLE] Complete message: $message")
                                processIncomingMessage(message)
                            }
                        }
                    }
                }
                ProtocolConstants.BATTERY_LEVEL_UUID -> {
                    if (data.isNotEmpty()) {
                        val level = data[0].toInt()
                        Log.d(tag, "[BLE] Battery Level: $level%")
                        _batteryLevel.value = level
                    }
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (descriptor.uuid == ProtocolConstants.CCCD_UUID) {
                    Log.d(tag, "[BLE] Notifications enabled, Controller READY")
                    _connectionState.value = ConnectionState.READY
                    isConnecting = false
                    handler.removeCallbacks(timeoutRunnable)
                    
                    // Request initial state after READY
                    sendCommand(ProtocolConstants.CMD_GET_STATE)
                    sendCommand(ProtocolConstants.CMD_INFO)
                    sendCommand(ProtocolConstants.CMD_GET_SETTINGS)
                }
            } else {
                Log.e(tag, "[BLE] Descriptor write failed: $status")
                handleError()
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Write successful")
            } else {
                Log.e(tag, "[BLE] Write failed: $status")
            }
        }
    }

    private fun processIncomingMessage(message: String) {
        if (message.isEmpty()) return
        
        // Update debug log with the latest message
        val currentLog = _receivedData.value
        val newLog = if (currentLog.length > 500) {
            message + "\n" + currentLog.substring(0, 400)
        } else {
            message + "\n" + currentLog
        }
        _receivedData.value = newLog
        
        if (message.startsWith("{")) {
            // Attempt to parse as different message types
            parser.parseState(message, _inputState.value.packetCount + 1)?.let {
                _inputState.value = it
                return
            }
            parser.parseDeviceInfo(message)?.let {
                Log.d(tag, "[BLE] Parsed Device Info: v${it.firmwareVersion}")
                _deviceInfo.value = it
                return
            }
            parser.parseDeviceSettings(message)?.let {
                Log.d(tag, "[BLE] Parsed Device Settings")
                _deviceSettings.value = it
                return
            }
        } else if (message == "PONG") {
            Log.d(tag, "[BLE] Received PONG")
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        val service = gatt.getService(ProtocolConstants.PSX_SERVICE_UUID)
        if (service != null) {
            Log.d(tag, "[BLE] PSXCore service found")
            txCharacteristic = service.getCharacteristic(ProtocolConstants.PSX_TX_UUID)
            rxCharacteristic = service.getCharacteristic(ProtocolConstants.PSX_RX_UUID)

            if (txCharacteristic != null) {
                gatt.setCharacteristicNotification(txCharacteristic!!, true)
                val descriptor = txCharacteristic!!.getDescriptor(ProtocolConstants.CCCD_UUID)
                if (descriptor != null) {
                    writeDescriptorCompat(gatt, descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                } else {
                    Log.e(tag, "[BLE] CCCD descriptor not found")
                    handleError()
                }
            } else {
                Log.e(tag, "[BLE] TX characteristic not found")
                handleError()
            }
            Log.d(tag, "[BLE] NUS Characteristics found and configured")
        } else {
            Log.w(tag, "[BLE] PSXCore custom service not found")
            handleError()
        }

        // Setup Battery Service if available
        val batteryService = gatt.getService(ProtocolConstants.BATTERY_SERVICE_UUID)
        if (batteryService != null) {
            Log.d(tag, "[BLE] Battery service found")
            batteryCharacteristic = batteryService.getCharacteristic(ProtocolConstants.BATTERY_LEVEL_UUID)
            if (batteryCharacteristic != null) {
                gatt.setCharacteristicNotification(batteryCharacteristic!!, true)
                // Note: In a production app with multiple notifications, we would queue these.
                // We prioritize the primary PSX TX characteristic above.
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeDescriptorCompat(
        gatt: BluetoothGatt,
        descriptor: BluetoothGattDescriptor,
        value: ByteArray
    ): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                descriptor.value = value
                gatt.writeDescriptor(descriptor)
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (isConnecting) {
            Log.w(tag, "[BLE] Connection already in progress")
            return
        }

        Log.d(tag, "[BLE] Reconnect requested for $address")
        closeGatt()

        val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter
        if (adapter == null || !adapter.isEnabled) {
            Log.e(tag, "[BLE] Bluetooth is disabled")
            _connectionState.value = ConnectionState.ERROR
            return
        }

        val device = adapter.getRemoteDevice(address)
        
        Log.d(tag, "[BLE] Creating new GATT connection to ${device.name ?: address}")
        _connectionState.value = ConnectionState.CONNECTING
        _connectedDeviceName.value = device.name
        
        isConnecting = true
        handler.postDelayed(timeoutRunnable, 15000)
        
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        Log.d(tag, "[BLE] Disconnect requested")
        closeGatt()
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    @SuppressLint("MissingPermission")
    fun sendCommand(command: String): Boolean {
        val rx = rxCharacteristic ?: return false
        val gatt = bluetoothGatt ?: return false
        val data = command.toByteArray(StandardCharsets.UTF_8)
        
        return writeCharacteristicCompat(gatt, rx, data)
    }

    @SuppressLint("MissingPermission")
    private fun writeCharacteristicCompat(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray
    ): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(
                characteristic,
                value,
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            ) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.value = value
                characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                gatt.writeCharacteristic(characteristic)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        Log.d(tag, "[BLE] Closing previous GATT")
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        txCharacteristic = null
        rxCharacteristic = null
    }

    private fun handleDisconnect() {
        closeGatt()
        _connectionState.value = ConnectionState.DISCONNECTED
        _connectedDeviceName.value = null
        resetInputState()
        isConnecting = false
        handler.removeCallbacks(timeoutRunnable)
    }

    private fun handleError() {
        closeGatt()
        _connectionState.value = ConnectionState.ERROR
        resetInputState()
        isConnecting = false
        handler.removeCallbacks(timeoutRunnable)
    }

    private fun resetInputState() {
        _inputState.value = ControllerInputState()
        _deviceInfo.value = DeviceInfo()
        _deviceSettings.value = DeviceSettings()
        _batteryLevel.value = null
        synchronized(messageBuffer) {
            messageBuffer.setLength(0)
        }
    }
}
