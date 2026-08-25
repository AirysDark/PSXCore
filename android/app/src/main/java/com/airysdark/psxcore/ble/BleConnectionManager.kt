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
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
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
    private val responseBuffer = StringBuilder()
    private val stateBuffer = StringBuilder()
    private val otaStatusBuffer = StringBuilder()

    private var isConnecting = false
    private var serviceDiscoveryRetried = false
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
            Log.d(tag, "[BLE] Connection change: ${gatt.device.address}, status=$status, state=$newState")
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "[BLE] GATT connection error: $status")
                handleError()
                return
            }
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.DISCOVERING_SERVICES
                    serviceDiscoveryRetried = false
                    if (!gatt.discoverServices()) {
                        Log.e(tag, "[BLE] discoverServices() returned false")
                        handleError()
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> handleDisconnect()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "[BLE] Service discovery failed: $status")
                handleError()
                return
            }

            Log.d(tag, "[BLE] Services discovered:")
            gatt.services.forEach { service ->
                Log.d(tag, "[BLE]   Service ${service.uuid}")
                service.characteristics.forEach { characteristic ->
                    Log.d(tag, "[BLE]     Characteristic ${characteristic.uuid}")
                }
            }

            val psxService = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
            if (psxService == null && !serviceDiscoveryRetried) {
                Log.w(tag, "[BLE] PSXCore service missing; retrying discovery once")
                serviceDiscoveryRetried = true
                handler.postDelayed({ gatt.discoverServices() }, 1000)
                return
            }

            if (psxService == null) {
                markCompanionMissing(gatt, "PSXCore service ${ProtocolConstants.PSXCORE_SERVICE_UUID} not found")
                return
            }

            _connectionState.value = ConnectionState.ENABLING_NOTIFICATIONS
            setupCharacteristics(gatt)
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
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "[BLE] Notification descriptor write failed for ${descriptor.characteristic.uuid}: $status")
                handleError()
                return
            }
            Log.d(tag, "[BLE] Notifications enabled: ${descriptor.characteristic.uuid}")
            processNextDescriptor(gatt)
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            val currentLabel = synchronized(textWriteQueue) {
                if (textWriteQueue.isNotEmpty()) textWriteQueue.first().label else characteristic.uuid.toString()
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Write OK [$currentLabel] -> ${characteristic.uuid}")
            } else {
                Log.e(tag, "[BLE] Write FAILED [$currentLabel] -> ${characteristic.uuid}, status=$status")
            }
            synchronized(textWriteQueue) {
                if (textWriteQueue.isNotEmpty()) textWriteQueue.removeFirst()
                textWriteInProgress = false
            }
            processNextTextWrite(gatt)
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        _isGamepadServiceReady.value = gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null

        val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
        if (service == null) {
            markCompanionMissing(gatt, "PSXCore service disappeared during setup")
            return
        }

        commandChar = service.getCharacteristic(ProtocolConstants.PSX_COMMAND_UUID)
        responseChar = service.getCharacteristic(ProtocolConstants.PSX_RESPONSE_UUID)
        controllerStateChar = service.getCharacteristic(ProtocolConstants.PSX_CONTROLLER_STATE_UUID)
        otaControlChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_CONTROL_UUID)
        otaDataChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_DATA_UUID)
        otaStatusChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_STATUS_UUID)

        val missing = buildList {
            if (commandChar == null) add("COMMAND ${ProtocolConstants.PSX_COMMAND_UUID}")
            if (responseChar == null) add("RESPONSE ${ProtocolConstants.PSX_RESPONSE_UUID}")
            if (controllerStateChar == null) add("CONTROLLER_STATE ${ProtocolConstants.PSX_CONTROLLER_STATE_UUID}")
            if (otaControlChar == null) add("OTA_CONTROL ${ProtocolConstants.PSX_OTA_CONTROL_UUID}")
            if (otaDataChar == null) add("OTA_DATA ${ProtocolConstants.PSX_OTA_DATA_UUID}")
            if (otaStatusChar == null) add("OTA_STATUS ${ProtocolConstants.PSX_OTA_STATUS_UUID}")
        }
        if (missing.isNotEmpty()) {
            markCompanionMissing(gatt, "GATT contract mismatch. Missing: ${missing.joinToString()}")
            return
        }

        Log.d(tag, "[BLE] PSXCore GATT contract validated: all 6 characteristics found")
        synchronized(descriptorQueue) {
            descriptorQueue.clear()
            listOf(responseChar!!, controllerStateChar!!, otaStatusChar!!).forEach { characteristic ->
                if (!gatt.setCharacteristicNotification(characteristic, true)) {
                    Log.e(tag, "[BLE] setCharacteristicNotification failed: ${characteristic.uuid}")
                    handleError()
                    return
                }
                val cccd = characteristic.getDescriptor(ProtocolConstants.CCCD_UUID)
                if (cccd == null) {
                    Log.e(tag, "[BLE] Missing CCCD for notify characteristic: ${characteristic.uuid}")
                    handleError()
                    return
                }
                descriptorQueue.add(cccd)
            }
        }
        processNextDescriptor(gatt)
    }

    @SuppressLint("MissingPermission")
    private fun processNextDescriptor(gatt: BluetoothGatt) {
        val descriptor = synchronized(descriptorQueue) {
            if (descriptorQueue.isEmpty()) null else descriptorQueue.removeAt(0)
        }
        if (descriptor == null) {
            Log.d(tag, "[BLE] GATT notifications ready; Controller READY")
            _connectionState.value = ConnectionState.READY
            _isCompanionServiceReady.value = true
            _isOtaReadyStatus.value = true
            isConnecting = false
            handler.removeCallbacks(timeoutRunnable)
            sendCommand(ProtocolConstants.CMD_GET_STATE)
            sendCommand(ProtocolConstants.CMD_INFO)
            sendCommand(ProtocolConstants.CMD_GET_SETTINGS)
            return
        }
        if (!writeDescriptorCompat(gatt, descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)) {
            Log.e(tag, "[BLE] Failed to start CCCD write for ${descriptor.characteristic.uuid}")
            handleError()
        }
    }

    private fun markCompanionMissing(gatt: BluetoothGatt, reason: String) {
        Log.e(tag, "[BLE] Companion unavailable: $reason")
        _connectionState.value = ConnectionState.COMPANION_MISSING
        _isCompanionServiceReady.value = false
        _isOtaReadyStatus.value = false
        _isGamepadServiceReady.value = gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null
        isConnecting = false
        handler.removeCallbacks(timeoutRunnable)
    }

    private fun handleIncomingData(data: ByteArray, uuid: UUID) {
        if (data.isEmpty()) return
        when (uuid) {
            ProtocolConstants.PSX_RESPONSE_UUID -> processStream(data, responseBuffer) { message ->
                Log.d(tag, "[BLE-RES] $message")
                _receivedData.value = message
                if (message.startsWith("{")) {
                    parser.parseDeviceInfo(message)?.let { _deviceInfo.value = it }
                    parser.parseDeviceSettings(message)?.let { _deviceSettings.value = it }
                } else if (message == "PONG") {
                    Log.d(tag, "[BLE] PONG received")
                }
            }
            ProtocolConstants.PSX_CONTROLLER_STATE_UUID -> processStream(data, stateBuffer) { message ->
                if (message.startsWith("{")) {
                    parser.parseState(message, _inputState.value.packetCount + 1)?.let { _inputState.value = it }
                }
            }
            ProtocolConstants.PSX_OTA_STATUS_UUID -> processStream(data, otaStatusBuffer) { message ->
                Log.d(tag, "[BLE-OTA] $message")
                if (message.startsWith("{")) {
                    parser.parseOtaReady(message)?.let { _otaReady.tryEmit(it) }
                    if (parser.parseOtaSuccess(message)) _otaResult.tryEmit(Result.success(Unit))
                    parser.parseOtaError(message)?.let { _otaResult.tryEmit(Result.failure(Exception(it))) }
                } else if (message == "OTA_READY") {
                    _otaReady.tryEmit(180)
                } else if (message == "OTA_SUCCESS") {
                    _otaResult.tryEmit(Result.success(Unit))
                }
            }
        }
    }

    private fun processStream(data: ByteArray, buffer: StringBuilder, onMessage: (String) -> Unit) {
        val chunk = String(data, StandardCharsets.UTF_8)
        synchronized(buffer) {
            if (buffer.length > 4096) {
                Log.w(tag, "[BLE] Stream buffer overflow; clearing")
                buffer.setLength(0)
            }
            buffer.append(chunk)
            var newlineIndex: Int
            while (buffer.indexOf("\n").also { newlineIndex = it } >= 0) {
                val message = buffer.substring(0, newlineIndex).trim()
                buffer.delete(0, newlineIndex + 1)
                if (message.isNotEmpty()) onMessage(message)
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (isConnecting) return
        closeGatt()
        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = manager.adapter ?: run {
            Log.e(tag, "[BLE] Bluetooth adapter unavailable")
            handleError()
            return
        }
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
        if (_connectionState.value != ConnectionState.READY) {
            Log.w(tag, "[BLE] Command rejected before READY: $command")
            return false
        }
        val characteristic = commandChar ?: run {
            Log.e(tag, "[BLE] COMMAND characteristic is null: $command")
            return false
        }
        return enqueueTextWrite(command, characteristic, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    fun sendOtaControl(command: String): Boolean {
        val characteristic = otaControlChar ?: run {
            Log.e(tag, "[BLE] OTA_CONTROL characteristic is null: $command")
            return false
        }
        return enqueueTextWrite(command, characteristic, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    private fun enqueueTextWrite(
        label: String,
        characteristic: BluetoothGattCharacteristic,
        writeType: Int
    ): Boolean {
        val gatt = bluetoothGatt ?: run {
            Log.e(tag, "[BLE] No GATT connection for write: $label")
            return false
        }
        val data = (if (label.endsWith("\n")) label else "$label\n").toByteArray(StandardCharsets.UTF_8)
        synchronized(textWriteQueue) {
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
            Log.e(tag, "[BLE] Failed to start write: ${next.label}")
            synchronized(textWriteQueue) {
                if (textWriteQueue.isNotEmpty()) textWriteQueue.removeFirst()
                textWriteInProgress = false
            }
            handler.post { processNextTextWrite(gatt) }
        } else {
            Log.d(tag, "[BLE] Command TX queued: ${next.label}")
        }
    }

    @SuppressLint("MissingPermission")
    suspend fun sendOtaData(data: ByteArray): Boolean {
        val characteristic = otaDataChar ?: run {
            Log.e(tag, "[BLE] OTA_DATA characteristic is null")
            return false
        }
        val gatt = bluetoothGatt ?: return false
        return writeCharacteristicCompat(gatt, characteristic, data, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
    }

    @SuppressLint("MissingPermission")
    private fun writeDescriptorCompat(
        gatt: BluetoothGatt,
        descriptor: BluetoothGattDescriptor,
        value: ByteArray
    ): Boolean = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        gatt.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS
    } else {
        @Suppress("DEPRECATION")
        run {
            descriptor.value = value
            gatt.writeDescriptor(descriptor)
        }
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
