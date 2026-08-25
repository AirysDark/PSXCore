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
    private val handler = Handler(Looper.getMainLooper())

    private var bluetoothGatt: BluetoothGatt? = null
    private var activeAttemptId = 0
    private var isConnecting = false
    private var serviceDiscoveryRetried = false
    private var serviceDiscoveryStarted = false
    private var currentTimeout: Runnable? = null

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

    private val descriptorQueue = ArrayDeque<BluetoothGattDescriptor>()

    private data class PendingWrite(
        val label: String,
        val characteristic: BluetoothGattCharacteristic,
        val value: ByteArray,
        val type: Int
    )

    private val writeQueue = ArrayDeque<PendingWrite>()
    private var writeInProgress = false

    private val parser = PsxCoreMessageParser()
    private val responseBuffer = StringBuilder()
    private val stateBuffer = StringBuilder()
    private val otaBuffer = StringBuilder()

    private fun isActiveGatt(gatt: BluetoothGatt): Boolean = bluetoothGatt === gatt

    private fun stale(gatt: BluetoothGatt, event: String): Boolean {
        if (!isActiveGatt(gatt)) {
            Log.w(tag, "[BLE] Ignoring stale $event callback")
            return true
        }
        return false
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (stale(gatt, "connection")) return

            val id = activeAttemptId
            Log.d(tag, "[BLE][$id] state=$newState status=$status")

            if (status != BluetoothGatt.GATT_SUCCESS) {
                handleError(id, gatt)
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.DISCOVERING_SERVICES
                    serviceDiscoveryStarted = false
                    serviceDiscoveryRetried = false
                    startTimeout(id, 12000)

                    handler.postDelayed({
                        if (isActiveGatt(gatt) &&
                            _connectionState.value == ConnectionState.DISCOVERING_SERVICES
                        ) {
                            startServiceDiscovery(gatt, id)
                        }
                    }, 300)

                    handler.postDelayed({
                        if (isActiveGatt(gatt)) {
                            gatt.requestMtu(512)
                        }
                    }, 800)
                }

                BluetoothProfile.STATE_DISCONNECTED -> handleDisconnect(id, gatt)
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (stale(gatt, "MTU")) return
            Log.d(tag, "[BLE][$activeAttemptId] MTU=$mtu status=$status")
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (stale(gatt, "services")) return

            val id = activeAttemptId
            serviceDiscoveryStarted = false

            if (status != BluetoothGatt.GATT_SUCCESS) {
                handleError(id, gatt)
                return
            }

            Log.d(
                tag,
                "[BLE][$id] Services discovered: ${gatt.services.joinToString { it.uuid.toString() }}"
            )

            val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)

            if (service == null && !serviceDiscoveryRetried) {
                serviceDiscoveryRetried = true
                Log.w(tag, "[BLE][$id] PSXCore service missing; retrying discovery once")
                handler.postDelayed({
                    if (isActiveGatt(gatt)) {
                        startServiceDiscovery(gatt, id)
                    }
                }, 1000)
                return
            }

            if (service == null) {
                _isGamepadServiceReady.value =
                    gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null
                _connectionState.value = ConnectionState.COMPANION_MISSING
                isConnecting = false
                cancelTimeout()
                return
            }

            _connectionState.value = ConnectionState.ENABLING_NOTIFICATIONS
            setupCharacteristics(gatt)
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            if (stale(gatt, "descriptor")) return

            if (status == BluetoothGatt.GATT_SUCCESS) {
                processNextDescriptor(gatt)
            } else {
                handleError(activeAttemptId, gatt)
            }
        }

        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            if (!stale(gatt, "characteristic") && Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                handleIncomingData(characteristic.value ?: return, characteristic.uuid)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            if (!stale(gatt, "characteristic")) {
                handleIncomingData(value, characteristic.uuid)
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (stale(gatt, "write")) return

            val completed = synchronized(writeQueue) {
                if (writeQueue.isNotEmpty()) writeQueue.removeFirst() else null
                    .also { writeInProgress = false }
            }

            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "[BLE] Write complete: ${completed?.label ?: "unknown"}")
            } else {
                Log.e(
                    tag,
                    "[BLE] Write failed status=$status label=${completed?.label ?: "unknown"} uuid=${characteristic.uuid}"
                )
            }

            processNextWrite(gatt)
        }
    }

    @SuppressLint("MissingPermission")
    private fun startServiceDiscovery(gatt: BluetoothGatt, id: Int) {
        if (!isActiveGatt(gatt) || id != activeAttemptId || serviceDiscoveryStarted) return

        serviceDiscoveryStarted = true
        Log.d(tag, "[BLE][$id] Starting service discovery")

        if (!gatt.discoverServices()) {
            serviceDiscoveryStarted = false
            handleError(id, gatt)
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        _isGamepadServiceReady.value =
            gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null

        val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
            ?: run {
                handleError(activeAttemptId, gatt)
                return
            }

        commandChar = service.getCharacteristic(ProtocolConstants.PSX_COMMAND_UUID)
        responseChar = service.getCharacteristic(ProtocolConstants.PSX_RESPONSE_UUID)
        controllerStateChar = service.getCharacteristic(ProtocolConstants.PSX_CONTROLLER_STATE_UUID)
        otaControlChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_CONTROL_UUID)
        otaDataChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_DATA_UUID)
        otaStatusChar = service.getCharacteristic(ProtocolConstants.PSX_OTA_STATUS_UUID)

        if (commandChar == null ||
            responseChar == null ||
            controllerStateChar == null ||
            otaControlChar == null ||
            otaDataChar == null ||
            otaStatusChar == null
        ) {
            Log.e(tag, "[BLE] Required PSXCore characteristics missing")
            handleError(activeAttemptId, gatt)
            return
        }

        descriptorQueue.clear()

        listOf(responseChar, controllerStateChar, otaStatusChar)
            .filterNotNull()
            .forEach { characteristic ->
                if (gatt.setCharacteristicNotification(characteristic, true)) {
                    characteristic.getDescriptor(ProtocolConstants.CCCD_UUID)
                        ?.let { descriptorQueue.addLast(it) }
                } else {
                    Log.e(tag, "[BLE] Failed to enable local notifications for ${characteristic.uuid}")
                    handleError(activeAttemptId, gatt)
                    return
                }
            }

        processNextDescriptor(gatt)
    }

    @SuppressLint("MissingPermission")
    private fun processNextDescriptor(gatt: BluetoothGatt) {
        val descriptor =
            if (descriptorQueue.isEmpty()) null else descriptorQueue.removeFirst()

        if (descriptor == null) {
            cancelTimeout()
            _connectionState.value = ConnectionState.READY
            _isCompanionServiceReady.value = true
            _isOtaReadyStatus.value = true
            isConnecting = false

            // Do not automatically send GET_STATE, INFO, or GET_SETTINGS here.
            // The four controller buttons now own their commands independently.
            Log.d(tag, "[BLE] Controller READY; waiting for user command")
            return
        }

        if (!writeDescriptorCompat(
                gatt,
                descriptor,
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            )
        ) {
            handleError(activeAttemptId, gatt)
        }
    }

    private fun processStream(
        data: ByteArray,
        buffer: StringBuilder,
        consume: (String) -> Unit
    ) {
        synchronized(buffer) {
            if (buffer.length > 4096) {
                Log.w(tag, "[BLE] Stream buffer overflow; clearing stale data")
                buffer.setLength(0)
            }

            buffer.append(String(data, StandardCharsets.UTF_8))

            while (true) {
                val newline = buffer.indexOf("\n")
                if (newline < 0) break

                val message = buffer.substring(0, newline).trim()
                buffer.delete(0, newline + 1)

                if (message.isNotEmpty()) {
                    message.replace("}{", "}\n{")
                        .split("\n")
                        .filter { it.isNotEmpty() }
                        .forEach { part ->
                            try {
                                consume(part)
                            } catch (e: Exception) {
                                Log.e(tag, "[BLE] Message processing failed: ${e.message}")
                            }
                        }
                }
            }
        }
    }

    private fun handleIncomingData(data: ByteArray, uuid: UUID) {
        when (uuid) {
            ProtocolConstants.PSX_RESPONSE_UUID -> {
                processStream(data, responseBuffer) { message ->
                    _receivedData.value = message
                    if (message.startsWith("{")) {
                        parser.parseDeviceInfo(message)?.let {
                            _deviceInfo.value = it
                        }
                        parser.parseDeviceSettings(message)?.let {
                            _deviceSettings.value = it
                        }
                    }
                }
            }

            ProtocolConstants.PSX_CONTROLLER_STATE_UUID -> {
                processStream(data, stateBuffer) { message ->
                    if (message.startsWith("{")) {
                        parser.parseState(
                            message,
                            _inputState.value.packetCount + 1,
                            _inputState.value.analogMode
                        )?.let {
                            _inputState.value = it
                        }
                    }
                }
            }

            ProtocolConstants.PSX_OTA_STATUS_UUID -> {
                processStream(data, otaBuffer) { message ->
                    if (message.startsWith("{")) {
                        parser.parseOtaStatus(message)
                    }
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (isConnecting ||
            (_connectionState.value == ConnectionState.READY &&
                bluetoothGatt?.device?.address == address)
        ) {
            return
        }

        activeAttemptId++
        val id = activeAttemptId
        cancelTimeout()
        closeGatt(false)

        val adapter =
            (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
                ?: run {
                    _connectionState.value = ConnectionState.ERROR
                    return
                }

        if (!adapter.isEnabled) {
            _connectionState.value = ConnectionState.ERROR
            return
        }

        _connectionState.value = ConnectionState.CONNECTING
        isConnecting = true
        serviceDiscoveryRetried = false
        serviceDiscoveryStarted = false

        val device = adapter.getRemoteDevice(address)
        _connectedDeviceName.value = device.name ?: address
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
        startTimeout(id, 12000)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        activeAttemptId++
        closeGatt(true)
        _connectionState.value = ConnectionState.DISCONNECTED
        _connectedDeviceName.value = null
        resetInputState()
    }

    fun sendCommand(command: String): Boolean {
        val characteristic = commandChar ?: return false
        return enqueueWrite(command, characteristic, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    fun sendOtaControl(command: String): Boolean {
        val characteristic = otaControlChar ?: return false
        return enqueueWrite(command, characteristic, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    @SuppressLint("MissingPermission")
    suspend fun sendOtaData(data: ByteArray): Boolean {
        val gatt = bluetoothGatt ?: return false
        val characteristic = otaDataChar ?: return false
        return writeCharacteristicCompat(
            gatt,
            characteristic,
            data,
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        )
    }

    private fun enqueueWrite(
        label: String,
        characteristic: BluetoothGattCharacteristic,
        type: Int
    ): Boolean {
        val gatt = bluetoothGatt ?: return false
        val normalizedLabel = label.trim()

        synchronized(writeQueue) {
            // Prevent the same command from being queued repeatedly, while still
            // allowing different buttons to queue their own independent commands.
            if (writeQueue.any {
                    it.label == normalizedLabel && it.characteristic.uuid == characteristic.uuid
                }
            ) {
                Log.d(tag, "[BLE] Duplicate queued command ignored: $normalizedLabel")
                return true
            }

            writeQueue.addLast(
                PendingWrite(
                    normalizedLabel,
                    characteristic,
                    (if (label.endsWith("\n")) label else "$label\n")
                        .toByteArray(StandardCharsets.UTF_8),
                    type
                )
            )
        }

        processNextWrite(gatt)
        return true
    }

    @SuppressLint("MissingPermission")
    private fun processNextWrite(gatt: BluetoothGatt) {
        if (!isActiveGatt(gatt)) return

        val next = synchronized(writeQueue) {
            if (writeInProgress || writeQueue.isEmpty()) {
                null
            } else {
                writeInProgress = true
                writeQueue.first()
            }
        } ?: return

        if (!writeCharacteristicCompat(gatt, next.characteristic, next.value, next.type)) {
            synchronized(writeQueue) {
                writeInProgress = false
            }

            handler.postDelayed({
                if (isActiveGatt(gatt)) {
                    processNextWrite(gatt)
                }
            }, 200)
        } else {
            Log.d(tag, "[BLE] Write started: ${next.label}")
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
    private fun writeCharacteristicCompat(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray,
        type: Int
    ): Boolean {
        characteristic.writeType = type

        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(characteristic, value, type) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.value = value
                gatt.writeCharacteristic(characteristic)
            }
        }
    }

    private fun startTimeout(id: Int, timeoutMs: Long) {
        cancelTimeout()
        currentTimeout = Runnable {
            if (id == activeAttemptId && isConnecting) {
                Log.e(tag, "[BLE][$id] setup timeout state=${_connectionState.value}")
                bluetoothGatt?.let { handleError(id, it) }
            }
        }
        handler.postDelayed(currentTimeout!!, timeoutMs)
    }

    private fun cancelTimeout() {
        currentTimeout?.let(handler::removeCallbacks)
        currentTimeout = null
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt(disconnect: Boolean) {
        val gatt = bluetoothGatt
        bluetoothGatt = null

        if (disconnect) {
            try {
                gatt?.disconnect()
            } catch (_: Exception) {
            }
        }

        try {
            gatt?.close()
        } catch (_: Exception) {
        }

        commandChar = null
        responseChar = null
        controllerStateChar = null
        otaControlChar = null
        otaDataChar = null
        otaStatusChar = null

        descriptorQueue.clear()
        synchronized(writeQueue) {
            writeQueue.clear()
            writeInProgress = false
        }

        _isGamepadServiceReady.value = false
        _isCompanionServiceReady.value = false
        _isOtaReadyStatus.value = false

        serviceDiscoveryStarted = false
        serviceDiscoveryRetried = false
        isConnecting = false
        cancelTimeout()
    }

    private fun handleDisconnect(id: Int, gatt: BluetoothGatt) {
        if (id == activeAttemptId && isActiveGatt(gatt)) {
            closeGatt(false)
            _connectionState.value = ConnectionState.DISCONNECTED
            _connectedDeviceName.value = null
            resetInputState()
        }
    }

    private fun handleError(id: Int, gatt: BluetoothGatt) {
        if (id == activeAttemptId && isActiveGatt(gatt)) {
            closeGatt(true)
            _connectionState.value = ConnectionState.ERROR
            _connectedDeviceName.value = null
            resetInputState()
        }
    }

    private fun resetInputState() {
        _inputState.value = ControllerInputState()
        _deviceInfo.value = DeviceInfo()
        _deviceSettings.value = DeviceSettings()
        _batteryLevel.value = null
        _receivedData.value = ""

        synchronized(responseBuffer) { responseBuffer.setLength(0) }
        synchronized(stateBuffer) { stateBuffer.setLength(0) }
        synchronized(otaBuffer) { otaBuffer.setLength(0) }
    }
}
