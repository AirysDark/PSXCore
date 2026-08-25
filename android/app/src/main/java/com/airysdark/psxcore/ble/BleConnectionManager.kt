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

    // 6 PSXCore Characteristics
    private var commandChar: BluetoothGattCharacteristic? = null
    private var responseChar: BluetoothGattCharacteristic? = null
    private var stateChar: BluetoothGattCharacteristic? = null
    private var otaControlChar: BluetoothGattCharacteristic? = null
    private var otaDataChar: BluetoothGattCharacteristic? = null
    private var otaStatusChar: BluetoothGattCharacteristic? = null

    private var pendingWrite: CompletableDeferred<Int>? = null

    private val parser = PsxCoreMessageParser()

    // Separate buffers for each NOTIFY stream
    private val responseBuffer = StringBuilder()
    private val stateBuffer = StringBuilder()
    private val otaStatusBuffer = StringBuilder()

    private var isConnecting = false
    private val handler = Handler(Looper.getMainLooper())
    private val timeoutRunnable = Runnable {
        if (isConnecting) {
            Log.e(tag, "[BLE] Connection timeout")
            handleError()
        }
    }

    private val descriptorQueue = mutableListOf<BluetoothGattDescriptor>()

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
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Descriptor write success: ${descriptor.characteristic.uuid}")
                processNextDescriptor(gatt)
            } else {
                Log.e(tag, "[BLE] Descriptor write failed: $status")
                handleError()
            }
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Write successful to ${characteristic.uuid}")
                pendingWrite?.complete(status)
            } else {
                Log.e(tag, "[BLE] Write failed: $status")
                pendingWrite?.completeExceptionally(RuntimeException("GATT Write failed: $status"))
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun processNextDescriptor(gatt: BluetoothGatt) {
        synchronized(descriptorQueue) {
            if (descriptorQueue.isNotEmpty()) {
                val descriptor = descriptorQueue.removeAt(0)
                writeDescriptorCompat(gatt, descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                Log.d(tag, "[BLE] All notifications enabled, Controller READY")
                _connectionState.value = ConnectionState.READY
                isConnecting = false
                handler.removeCallbacks(timeoutRunnable)
                
                // Request initial state after READY
                sendCommand(ProtocolConstants.CMD_GET_STATE)
                sendCommand(ProtocolConstants.CMD_INFO)
            }
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
            ProtocolConstants.PSX_STATE_UUID -> {
                processStream(data, stateBuffer) { message ->
                    if (message.startsWith("{")) {
                        parser.parseState(message, _inputState.value.packetCount + 1)?.let {
                            _inputState.value = it
                        }
                    }
                }
            }
            ProtocolConstants.PSX_OTA_STATUS_UUID -> {
                processStream(data, otaStatusBuffer) { message ->
                    Log.d(tag, "[BLE-OTA] Status: $message")
                    if (message.startsWith("{")) {
                        parser.parseOtaReady(message)?.let { _otaReady.tryEmit(it) }
                        if (parser.parseOtaSuccess(message)) _otaResult.tryEmit(Result.success(Unit))
                        parser.parseOtaError(message)?.let { _otaResult.tryEmit(Result.failure(Exception(it))) }
                    } else if (message.startsWith("OTA_PROGRESS:")) {
                        // Handle raw text progress if needed
                    } else if (message == "OTA_READY") {
                        _otaReady.tryEmit(180) // Default chunk size if not in JSON
                    } else if (message == "OTA_SUCCESS") {
                        _otaResult.tryEmit(Result.success(Unit))
                    }
                }
            }
        }
    }

    private fun processStream(data: ByteArray, buffer: StringBuilder, onMessage: (String) -> Unit) {
        val chunk = String(data, StandardCharsets.UTF_8)
        synchronized(buffer) {
            buffer.append(chunk)
            var newlineIndex: Int
            while (buffer.indexOf("\n").also { newlineIndex = it } >= 0) {
                val message = buffer.substring(0, newlineIndex).trim()
                buffer.delete(0, newlineIndex + 1)
                if (message.isNotEmpty()) {
                    onMessage(message)
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
        if (service != null) {
            Log.d(tag, "[BLE] PSXCore service found")
            commandChar = service.getCharacteristic(ProtocolConstants.PSX_COMMAND_UUID)
            responseChar = service.getCharacteristic(ProtocolConstants.PSX_RESPONSE_UUID)
            stateChar = service.getCharacteristic(ProtocolConstants.PSX_STATE_UUID)
            otaControlChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_CONTROL_UUID)
            otaDataChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_DATA_UUID)
            otaStatusChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_STATUS_UUID)

            // Queue up notifications
            synchronized(descriptorQueue) {
                descriptorQueue.clear()
                listOf(responseChar, stateChar, otaStatusChar).filterNotNull().forEach { char ->
                    gatt.setCharacteristicNotification(char, true)
                    char.getDescriptor(ProtocolConstants.CCCD_UUID)?.let { descriptorQueue.add(it) }
                }
            }
            processNextDescriptor(gatt)
        } else {
            Log.w(tag, "[BLE] PSXCore service not found")
            handleError()
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
        if (isConnecting) return
        closeGatt()
        val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter ?: return
        val device = adapter.getRemoteDevice(address)
        _connectionState.value = ConnectionState.CONNECTING
        _connectedDeviceName.value = device.name
        isConnecting = true
        handler.postDelayed(timeoutRunnable, 15000)
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        closeGatt()
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    @SuppressLint("MissingPermission")
    fun sendCommand(command: String): Boolean {
        val char = commandChar ?: return false
        val gatt = bluetoothGatt ?: return false
        val data = (if (command.endsWith("\n")) command else "$command\n").toByteArray(StandardCharsets.UTF_8)
        return writeCharacteristicCompat(gatt, char, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    fun sendOtaControl(command: String): Boolean {
        val char = otaControlChar ?: return false
        val gatt = bluetoothGatt ?: return false
        val data = (if (command.endsWith("\n")) command else "$command\n").toByteArray(StandardCharsets.UTF_8)
        return writeCharacteristicCompat(gatt, char, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    suspend fun sendOtaData(data: ByteArray): Boolean {
        val char = otaDataChar ?: return false
        val gatt = bluetoothGatt ?: return false
        
        // Use NO_RESPONSE for faster transfer as requested.
        // For raw binary we don't necessarily need to wait for write completion if the stack handles it.
        // But for flow control, we could wait if needed.
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
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        commandChar = null
        responseChar = null
        stateChar = null
        otaControlChar = null
        otaDataChar = null
        otaStatusChar = null
        synchronized(responseBuffer) { responseBuffer.setLength(0) }
        synchronized(stateBuffer) { stateBuffer.setLength(0) }
        synchronized(otaStatusBuffer) { otaStatusBuffer.setLength(0) }
        synchronized(descriptorQueue) { descriptorQueue.clear() }
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
    }
}
