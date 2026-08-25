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
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withTimeoutOrNull
import java.nio.charset.StandardCharsets
import java.util.UUID

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    DISCOVERING_SERVICES,
    ENABLING_NOTIFICATIONS,
    READY,
    ERROR,
    COMPANION_MISSING
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

    private val _isGamepadServiceReady = MutableStateFlow(false)
    val isGamepadServiceReady: StateFlow<Boolean> = _isGamepadServiceReady.asStateFlow()

    private val _isCompanionServiceReady = MutableStateFlow(false)
    val isCompanionServiceReady: StateFlow<Boolean> = _isCompanionServiceReady.asStateFlow()

    private val _isOtaReadyStatus = MutableStateFlow(false)
    val isOtaReadyStatus: StateFlow<Boolean> = _isOtaReadyStatus.asStateFlow()

    private val _otaReady = MutableSharedFlow<Int>(extraBufferCapacity = 1)
    val otaReady = _otaReady.asSharedFlow()

    private val _otaResult = MutableSharedFlow<Result<Unit>>(extraBufferCapacity = 1)
    val otaResult = _otaResult.asSharedFlow()

    // 6 PSXCore Characteristics
    private var commandChar: BluetoothGattCharacteristic? = null
    private var responseChar: BluetoothGattCharacteristic? = null
    private var controllerStateChar: BluetoothGattCharacteristic? = null
    private var otaControlChar: BluetoothGattCharacteristic? = null
    private var otaDataChar: BluetoothGattCharacteristic? = null
    private var otaStatusChar: BluetoothGattCharacteristic? = null

    private data class PendingTextWrite(
        val label: String,
        val characteristic: BluetoothGattCharacteristic,
        val value: ByteArray,
        val writeType: Int
    )

    private val textWriteQueue = ArrayDeque<PendingTextWrite>()
    private var textWriteInProgress = false

    private val parser = PsxCoreMessageParser()

    // Separate buffers for each NOTIFY stream
    private val responseBuffer = StringBuilder()
    private val stateBuffer = StringBuilder()
    private val otaStatusBuffer = StringBuilder()

    private var isConnecting = false
    private var activeAttemptId = 0
    private var serviceDiscoveryRetried = false
    private val handler = Handler(Looper.getMainLooper())
    
    private class TimeoutRunnable(val attemptId: Int, val manager: BleConnectionManager) : Runnable {
        override fun run() {
            manager.onConnectionTimeout(attemptId)
        }
    }

    private var currentTimeout: TimeoutRunnable? = null

    private val descriptorQueue = mutableListOf<BluetoothGattDescriptor>()

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            val address = gatt.device.address
            val attemptId = activeAttemptId
            Log.d(tag, "[BLE][$attemptId] onConnectionStateChange: address=$address, status=$status, newState=$newState")

            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "[BLE][$attemptId] GATT error: $status")
                handleError(attemptId)
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.d(tag, "[BLE][$attemptId] STATE_CONNECTED. Cancelling timeout.")
                    handler.removeCallbacksAndMessages(null)
                    currentTimeout = null
                    
                    _connectionState.value = ConnectionState.DISCOVERING_SERVICES
                    serviceDiscoveryRetried = false
                    Log.d(tag, "[BLE][$attemptId] Starting service discovery")
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.d(tag, "[BLE][$attemptId] STATE_DISCONNECTED")
                    handleDisconnect(attemptId)
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val attemptId = activeAttemptId
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE][$attemptId] Services discovered")
                
                gatt.requestMtu(512)
                
                Log.d(tag, "[BLE][$attemptId] Discovered service UUIDs:")
                gatt.services.forEach { service ->
                    Log.d(tag, "[BLE][$attemptId]   - ${service.uuid}")
                }

                val psxService = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
                if (psxService == null && !serviceDiscoveryRetried) {
                    Log.w(tag, "[BLE][$attemptId] PSXCore service not found. Retrying discovery in 1s (stale cache?)")
                    serviceDiscoveryRetried = true
                    handler.postDelayed({
                        if (activeAttemptId == attemptId) {
                            gatt.discoverServices()
                        }
                    }, 1000)
                    return
                }

                if (psxService != null) {
                    Log.d(tag, "[BLE][$attemptId] PSXCore service found. Enabling notifications.")
                    _connectionState.value = ConnectionState.ENABLING_NOTIFICATIONS
                    setupCharacteristics(gatt)
                } else {
                    Log.e(tag, "[BLE][$attemptId] PSXCore custom service MISSING after retry! UUID: ${ProtocolConstants.PSXCORE_SERVICE_UUID}")
                    _connectionState.value = ConnectionState.COMPANION_MISSING
                    val hidService = gatt.getService(ProtocolConstants.HID_SERVICE_UUID)
                    _isGamepadServiceReady.value = hidService != null
                    isConnecting = false
                }
            } else {
                Log.e(tag, "[BLE][$attemptId] Service discovery failed with status: $status")
                handleError(attemptId)
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            Log.d(tag, "[BLE][${activeAttemptId}] MTU changed to $mtu, status=$status")
        }

        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                handleIncomingData(characteristic.value ?: return, characteristic.uuid)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleIncomingData(value, characteristic.uuid)
        }

        @SuppressLint("MissingPermission")
        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            val attemptId = activeAttemptId
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE][$attemptId] Descriptor write success: ${descriptor.characteristic.uuid}")
                processNextDescriptor(gatt)
            } else {
                Log.e(tag, "[BLE][$attemptId] Descriptor write failed: $status")
                handleError(attemptId)
            }
        }

        @SuppressLint("MissingPermission")
        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            val currentLabel = synchronized(textWriteQueue) {
                if (textWriteQueue.isNotEmpty()) textWriteQueue.first().label else "unknown"
            }
            
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Write successful to ${characteristic.uuid} ($currentLabel)")
            } else {
                Log.e(tag, "[BLE] Write failed to ${characteristic.uuid} ($currentLabel): $status")
            }
            
            synchronized(textWriteQueue) {
                if (textWriteQueue.isNotEmpty()) {
                    textWriteQueue.removeFirst()
                }
                textWriteInProgress = false
            }
            processNextTextWrite(gatt)
        }
    }

    @SuppressLint("MissingPermission")
    private fun processNextDescriptor(gatt: BluetoothGatt) {
        val attemptId = activeAttemptId
        val descriptor = synchronized(descriptorQueue) {
            if (descriptorQueue.isNotEmpty()) descriptorQueue.removeAt(0) else null
        }
        
        if (descriptor != null) {
            if (!writeDescriptorCompat(gatt, descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)) {
                Log.e(tag, "[BLE][$attemptId] Failed to start descriptor write")
                handleError(attemptId)
            }
        } else {
            Log.d(tag, "[BLE][$attemptId] All notifications enabled, Controller READY")
            _connectionState.value = ConnectionState.READY
            _isCompanionServiceReady.value = true
            _isOtaReadyStatus.value = true
            isConnecting = false
            
            // Request initial state after READY with spacing to avoid collision
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_GET_STATE) }, 200)
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_INFO) }, 800)
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_GET_SETTINGS) }, 1400)
        }
    }

    private fun handleIncomingData(data: ByteArray, uuid: UUID) {
        if (data.isEmpty()) return

        when (uuid) {
            ProtocolConstants.PSX_RESPONSE_UUID -> {
                processStream(data, responseBuffer) { message ->
                    Log.d(tag, "[BLE-RES] Incoming: $message")
                    _receivedData.value = message
                    if (message.startsWith("{")) {
                        parser.parseDeviceInfo(message)?.let { _deviceInfo.value = it }
                        parser.parseDeviceSettings(message)?.let { _deviceSettings.value = it }
                    } else if (message == "PONG") {
                        Log.d(tag, "[BLE] Received PONG")
                    }
                }
            }
            ProtocolConstants.PSX_CONTROLLER_STATE_UUID -> {
                processStream(data, stateBuffer) { message ->
                    if (message.startsWith("{")) {
                        parser.parseState(message, _inputState.value.packetCount + 1, _inputState.value.analogMode)?.let {
                            _inputState.value = it
                        }
                    }
                }
            }
            ProtocolConstants.PSX_OTA_STATUS_UUID -> {
                processStream(data, otaStatusBuffer) { message ->
                    Log.d(tag, "[BLE-OTA] Status: $message")
                    if (message.startsWith("{")) {
                        parser.parseOtaStatus(message)?.let { status ->
                            // Update OTA state in update manager
                        }
                    }
                }
            }
        }
    }

    private fun processStream(data: ByteArray, buffer: StringBuilder, onMessage: (String) -> Unit) {
        val chunk = String(data, StandardCharsets.UTF_8)
        synchronized(buffer) {
            if (buffer.length > 4096) {
                Log.w(tag, "[BLE] Stream buffer overflow ($tag); clearing stale data. Current buffer: ${buffer.toString()}")
                buffer.setLength(0)
            }

            buffer.append(chunk)
            
            var newlineIndex: Int
            while (buffer.indexOf("\n").also { newlineIndex = it } >= 0) {
                val message = buffer.substring(0, newlineIndex).trim()
                buffer.delete(0, newlineIndex + 1)
                
                if (message.isNotEmpty()) {
                    if (message.contains("}{")) {
                        Log.w(tag, "[BLE] Detected merged messages, splitting: $message")
                        val parts = message.replace("}{", "}\n{").split("\n")
                        parts.forEach { part ->
                            if (part.isNotEmpty()) {
                                try { onMessage(part) } catch (e: Exception) { Log.e(tag, "[BLE] Error: ${e.message}") }
                            }
                        }
                    } else {
                        try { onMessage(message) } catch (e: Exception) { Log.e(tag, "[BLE] Error: ${e.message}") }
                    }
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        _isGamepadServiceReady.value = gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null

        val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
        if (service != null) {
            Log.d(tag, "[BLE][${activeAttemptId}] PSXCore custom service found: ${service.uuid}")
            commandChar = service.getCharacteristic(ProtocolConstants.PSX_COMMAND_UUID)
            responseChar = service.getCharacteristic(ProtocolConstants.PSX_RESPONSE_UUID)
            controllerStateChar = service.getCharacteristic(ProtocolConstants.PSX_CONTROLLER_STATE_UUID)
            otaControlChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_CONTROL_UUID)
            otaDataChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_DATA_UUID)
            otaStatusChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_STATUS_UUID)

            synchronized(descriptorQueue) {
                descriptorQueue.clear()
                listOf(responseChar, controllerStateChar, otaStatusChar).filterNotNull().forEach { char ->
                    gatt.setCharacteristicNotification(char, true)
                    char.getDescriptor(ProtocolConstants.CCCD_UUID)?.let { descriptorQueue.add(it) }
                }
            }
            processNextDescriptor(gatt)
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
            Log.d(tag, "[BLE] connect() ignored - already connecting")
            return
        }
        
        if (_connectionState.value == ConnectionState.READY && bluetoothGatt?.device?.address == address) {
            Log.d(tag, "[BLE] connect() ignored - already connected to this device")
            return
        }

        activeAttemptId++
        val attemptId = activeAttemptId
        Log.d(tag, "[BLE][$attemptId] Reconnect requested for $address")
        
        closeGatt()
        
        val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter ?: run {
            Log.e(tag, "[BLE][$attemptId] Bluetooth adapter unavailable")
            _connectionState.value = ConnectionState.ERROR
            return
        }
        
        if (!adapter.isEnabled) {
            Log.e(tag, "[BLE][$attemptId] Bluetooth adapter disabled")
            _connectionState.value = ConnectionState.ERROR
            return
        }

        val device = adapter.getRemoteDevice(address)
        
        Log.d(tag, "[BLE][$attemptId] Creating new GATT connection to ${device.name ?: address}")
        _connectionState.value = ConnectionState.CONNECTING
        _connectedDeviceName.value = device.name
        
        isConnecting = true
        currentTimeout = TimeoutRunnable(attemptId, this)
        handler.postDelayed(currentTimeout!!, 15000)
        
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    fun onConnectionTimeout(attemptId: Int) {
        if (activeAttemptId == attemptId && isConnecting) {
            Log.e(tag, "[BLE][$attemptId] Connection timeout reached")
            handleError(attemptId)
        }
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        Log.d(tag, "[BLE] Disconnect requested")
        closeGatt()
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    @SuppressLint("MissingPermission")
    fun sendCommand(command: String): Boolean {
        val char = commandChar ?: return false
        return enqueueTextWrite(command, char, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    fun sendOtaControl(command: String): Boolean {
        val char = otaControlChar ?: return false
        return enqueueTextWrite(command, char, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    private fun enqueueTextWrite(
        label: String,
        characteristic: BluetoothGattCharacteristic,
        writeType: Int
    ): Boolean {
        val gatt = bluetoothGatt ?: return false
        val data = (if (label.endsWith("\n")) label else "$label\n").toByteArray(StandardCharsets.UTF_8)
        synchronized(textWriteQueue) {
            if (textWriteQueue.any { it.label == label.trim() && it.characteristic == characteristic }) {
                Log.d(tag, "[BLE] Command already in queue, skipping: $label")
                return true
            }
            textWriteQueue.addLast(PendingTextWrite(label.trim(), characteristic, data, writeType))
        }
        processNextTextWrite(gatt)
        return true
    }

    @SuppressLint("MissingPermission")
    private fun processNextTextWrite(gatt: BluetoothGatt) {
        val next = synchronized(textWriteQueue) {
            if (textWriteInProgress || textWriteQueue.isEmpty()) null
            else {
                textWriteInProgress = true
                textWriteQueue.first()
            }
        } ?: return

        val started = writeCharacteristicCompat(gatt, next.characteristic, next.value, next.writeType)
        if (!started) {
            Log.e(tag, "[BLE] Failed to start write (BUSY or error): ${next.label}. Retrying later.")
            // Do NOT remove it from queue, just mark not in progress and retry
            synchronized(textWriteQueue) {
                textWriteInProgress = false
            }
            handler.postDelayed({
                val currentGatt = bluetoothGatt
                if (currentGatt != null) processNextTextWrite(currentGatt)
            }, 200)
        } else {
            Log.d(tag, "[BLE] Command TX started: ${next.label}")
        }
    }

    @SuppressLint("MissingPermission")
    suspend fun sendOtaData(data: ByteArray): Boolean {
        val char = otaDataChar ?: return false
        val gatt = bluetoothGatt ?: return false
        return writeCharacteristicCompat(gatt, char, data, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
    }

    @SuppressLint("MissingPermission")
    private fun writeCharacteristicCompat(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray,
        writeType: Int
    ): Boolean {
        characteristic.writeType = writeType
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(characteristic, value, writeType) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.value = value
                gatt.writeCharacteristic(characteristic)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        Log.d(tag, "[BLE] Closing GATT connection (Attempt ID: $activeAttemptId)")
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        commandChar = null
        responseChar = null
        controllerStateChar = null
        otaControlChar = null
        otaDataChar = null
        otaStatusChar = null
        _isGamepadServiceReady.value = false
        _isCompanionServiceReady.value = false
        _isOtaReadyStatus.value = false
        synchronized(responseBuffer) { responseBuffer.setLength(0) }
        synchronized(stateBuffer) { stateBuffer.setLength(0) }
        synchronized(otaStatusBuffer) { otaStatusBuffer.setLength(0) }
        synchronized(descriptorQueue) { descriptorQueue.clear() }
        synchronized(textWriteQueue) {
            textWriteQueue.clear()
            textWriteInProgress = false
        }
        handler.removeCallbacksAndMessages(null)
        currentTimeout = null
        isConnecting = false
    }

    private fun handleDisconnect(attemptId: Int) {
        if (activeAttemptId == attemptId) {
            closeGatt()
            _connectionState.value = ConnectionState.DISCONNECTED
            _connectedDeviceName.value = null
            resetInputState()
        }
    }

    private fun handleError(attemptId: Int) {
        if (activeAttemptId == attemptId) {
            closeGatt()
            _connectionState.value = ConnectionState.ERROR
            resetInputState()
        }
    }

    private fun resetInputState() {
        _inputState.value = ControllerInputState()
        _deviceInfo.value = DeviceInfo()
        _deviceSettings.value = DeviceSettings()
        _batteryLevel.value = null
        synchronized(responseBuffer) { responseBuffer.setLength(0) }
        synchronized(stateBuffer) { stateBuffer.setLength(0) }
        synchronized(otaStatusBuffer) { otaStatusBuffer.setLength(0) }
    }
}
