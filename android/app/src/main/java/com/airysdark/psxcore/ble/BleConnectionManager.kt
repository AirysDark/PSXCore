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

enum class ConnectionState { DISCONNECTED, CONNECTING, DISCOVERING_SERVICES, ENABLING_NOTIFICATIONS, READY, ERROR, COMPANION_MISSING }

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
    private data class PendingWrite(val label: String, val characteristic: BluetoothGattCharacteristic, val value: ByteArray, val type: Int)
    private val writeQueue = ArrayDeque<PendingWrite>()
    private var writeInProgress = false

    private val parser = PsxCoreMessageParser()
    private val responseBuffer = StringBuilder()
    private val stateBuffer = StringBuilder()
    private val otaBuffer = StringBuilder()

    private fun isActiveGatt(gatt: BluetoothGatt) = bluetoothGatt === gatt
    private fun stale(gatt: BluetoothGatt, event: String): Boolean {
        if (!isActiveGatt(gatt)) { Log.w(tag, "[BLE] Ignoring stale $event callback") ; return true }
        return false
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (stale(gatt, "connection")) return
            val id = activeAttemptId
            Log.d(tag, "[BLE][$id] state=$newState status=$status")
            if (status != BluetoothGatt.GATT_SUCCESS) { handleError(id, gatt); return }
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.DISCOVERING_SERVICES
                    serviceDiscoveryStarted = false; serviceDiscoveryRetried = false
                    startTimeout(id, 12000)
                    handler.postDelayed({ if (isActiveGatt(gatt) && _connectionState.value == ConnectionState.DISCOVERING_SERVICES) startServiceDiscovery(gatt, id) }, 300)
                    handler.postDelayed({ if (isActiveGatt(gatt)) gatt.requestMtu(512) }, 800)
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
            if (status != BluetoothGatt.GATT_SUCCESS) { handleError(id, gatt); return }
            Log.d(tag, "[BLE][$id] Services discovered: ${gatt.services.joinToString { it.uuid.toString() }}")
            val service = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID)
            if (service == null && !serviceDiscoveryRetried) {
                serviceDiscoveryRetried = true
                Log.w(tag, "[BLE][$id] PSXCore service missing; retrying discovery once")
                handler.postDelayed({ if (isActiveGatt(gatt)) startServiceDiscovery(gatt, id) }, 1000)
                return
            }
            if (service == null) {
                _isGamepadServiceReady.value = gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null
                _connectionState.value = ConnectionState.COMPANION_MISSING
                isConnecting = false; cancelTimeout()
                return
            }
            _connectionState.value = ConnectionState.ENABLING_NOTIFICATIONS
            setupCharacteristics(gatt)
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (stale(gatt, "descriptor")) return
            if (status == BluetoothGatt.GATT_SUCCESS) processNextDescriptor(gatt) else handleError(activeAttemptId, gatt)
        }

        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (!stale(gatt, "characteristic") && Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) handleIncomingData(characteristic.value ?: return, characteristic.uuid)
        }
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (!stale(gatt, "characteristic")) handleIncomingData(value, characteristic.uuid)
        }
        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (stale(gatt, "write")) return
            synchronized(writeQueue) { if (writeQueue.isNotEmpty()) writeQueue.removeFirst(); writeInProgress = false }
            if (status != BluetoothGatt.GATT_SUCCESS) Log.e(tag, "[BLE] write failed status=$status uuid=${characteristic.uuid}")
            processNextWrite(gatt)
        }
    }

    @SuppressLint("MissingPermission")
    private fun startServiceDiscovery(gatt: BluetoothGatt, id: Int) {
        if (!isActiveGatt(gatt) || id != activeAttemptId || serviceDiscoveryStarted) return
        serviceDiscoveryStarted = true
        if (!gatt.discoverServices()) { serviceDiscoveryStarted = false; handleError(id, gatt) }
    }

    @SuppressLint("MissingPermission")
    private fun setupCharacteristics(gatt: BluetoothGatt) {
        _isGamepadServiceReady.value = gatt.getService(ProtocolConstants.HID_SERVICE_UUID) != null
        val s = gatt.getService(ProtocolConstants.PSXCORE_SERVICE_UUID) ?: run { handleError(activeAttemptId, gatt); return }
        commandChar = s.getCharacteristic(ProtocolConstants.PSX_COMMAND_UUID)
        responseChar = s.getCharacteristic(ProtocolConstants.PSX_RESPONSE_UUID)
        controllerStateChar = s.getCharacteristic(ProtocolConstants.PSX_CONTROLLER_STATE_UUID)
        otaControlChar = s.getCharacteristic(ProtocolConstants.PSX_OTA_CONTROL_UUID)
        otaDataChar = s.getCharacteristic(ProtocolConstants.PSX_OTA_DATA_UUID)
        otaStatusChar = s.getCharacteristic(ProtocolConstants.PSX_OTA_STATUS_UUID)
        if (commandChar == null || responseChar == null || controllerStateChar == null || otaControlChar == null || otaDataChar == null || otaStatusChar == null) { Log.e(tag, "[BLE] Required PSXCore characteristics missing"); handleError(activeAttemptId, gatt); return }
        descriptorQueue.clear()
        listOf(responseChar, controllerStateChar, otaStatusChar).filterNotNull().forEach { c ->
            if (gatt.setCharacteristicNotification(c, true)) c.getDescriptor(ProtocolConstants.CCCD_UUID)?.let { descriptorQueue.addLast(it) }
        }
        processNextDescriptor(gatt)
    }

    @SuppressLint("MissingPermission")
    private fun processNextDescriptor(gatt: BluetoothGatt) {
        val d = if (descriptorQueue.isEmpty()) null else descriptorQueue.removeFirst()
        if (d == null) {
            cancelTimeout(); _connectionState.value = ConnectionState.READY; _isCompanionServiceReady.value = true; _isOtaReadyStatus.value = true; isConnecting = false
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_GET_STATE) }, 200)
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_INFO) }, 800)
            handler.postDelayed({ sendCommand(ProtocolConstants.CMD_GET_SETTINGS) }, 1400)
            return
        }
        if (!writeDescriptorCompat(gatt, d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)) handleError(activeAttemptId, gatt)
    }

    private fun processStream(data: ByteArray, buffer: StringBuilder, consume: (String) -> Unit) {
        synchronized(buffer) {
            if (buffer.length > 4096) buffer.setLength(0)
            buffer.append(String(data, StandardCharsets.UTF_8))
            while (true) {
                val n = buffer.indexOf("\n"); if (n < 0) break
                val msg = buffer.substring(0, n).trim(); buffer.delete(0, n + 1)
                if (msg.isNotEmpty()) msg.replace("}{", "}\n{").split("\n").forEach(consume)
            }
        }
    }

    private fun handleIncomingData(data: ByteArray, uuid: UUID) = when (uuid) {
        ProtocolConstants.PSX_RESPONSE_UUID -> processStream(data, responseBuffer) { m -> _receivedData.value = m; if (m.startsWith("{")) { parser.parseDeviceInfo(m)?.let { _deviceInfo.value = it }; parser.parseDeviceSettings(m)?.let { _deviceSettings.value = it } } }
        ProtocolConstants.PSX_CONTROLLER_STATE_UUID -> processStream(data, stateBuffer) { m -> if (m.startsWith("{")) parser.parseState(m, _inputState.value.packetCount + 1, _inputState.value.analogMode)?.let { _inputState.value = it } }
        ProtocolConstants.PSX_OTA_STATUS_UUID -> processStream(data, otaBuffer) { m -> if (m.startsWith("{")) parser.parseOtaStatus(m) }
        else -> Unit
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (isConnecting || (_connectionState.value == ConnectionState.READY && bluetoothGatt?.device?.address == address)) return
        activeAttemptId++; val id = activeAttemptId; cancelTimeout(); closeGatt(false)
        val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter ?: run { _connectionState.value = ConnectionState.ERROR; return }
        if (!adapter.isEnabled) { _connectionState.value = ConnectionState.ERROR; return }
        _connectionState.value = ConnectionState.CONNECTING; isConnecting = true; serviceDiscoveryRetried = false; serviceDiscoveryStarted = false
        bluetoothGatt = adapter.getRemoteDevice(address).connectGatt(context, false, gattCallback)
        _connectedDeviceName.value = bluetoothGatt?.device?.name
        startTimeout(id, 12000)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() { activeAttemptId++; closeGatt(true); _connectionState.value = ConnectionState.DISCONNECTED; _connectedDeviceName.value = null; resetInputState() }

    fun sendCommand(command: String): Boolean = commandChar?.let { enqueueWrite(command, it, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) } ?: false
    fun sendOtaControl(command: String): Boolean = otaControlChar?.let { enqueueWrite(command, it, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) } ?: false
    @SuppressLint("MissingPermission")
    suspend fun sendOtaData(data: ByteArray): Boolean { val g = bluetoothGatt ?: return false; val c = otaDataChar ?: return false; return writeCharacteristicCompat(g, c, data, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) }

    private fun enqueueWrite(label: String, c: BluetoothGattCharacteristic, type: Int): Boolean {
        val g = bluetoothGatt ?: return false; synchronized(writeQueue) { writeQueue.addLast(PendingWrite(label.trim(), c, (if (label.endsWith("\n")) label else "$label\n").toByteArray(StandardCharsets.UTF_8), type)) }; processNextWrite(g); return true
    }
    @SuppressLint("MissingPermission")
    private fun processNextWrite(g: BluetoothGatt) {
        if (!isActiveGatt(g)) return
        val next = synchronized(writeQueue) { if (writeInProgress || writeQueue.isEmpty()) null else { writeInProgress = true; writeQueue.first() } } ?: return
        if (!writeCharacteristicCompat(g, next.characteristic, next.value, next.type)) { synchronized(writeQueue) { writeInProgress = false }; handler.postDelayed({ if (isActiveGatt(g)) processNextWrite(g) }, 200) }
    }

    @SuppressLint("MissingPermission")
    private fun writeDescriptorCompat(g: BluetoothGatt, d: BluetoothGattDescriptor, value: ByteArray) = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) g.writeDescriptor(d, value) == BluetoothStatusCodes.SUCCESS else { @Suppress("DEPRECATION") d.value = value; @Suppress("DEPRECATION") g.writeDescriptor(d) }
    @SuppressLint("MissingPermission")
    private fun writeCharacteristicCompat(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, type: Int): Boolean { c.writeType = type; return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) g.writeCharacteristic(c, value, type) == BluetoothStatusCodes.SUCCESS else { @Suppress("DEPRECATION") c.value = value; @Suppress("DEPRECATION") g.writeCharacteristic(c) } }

    private fun startTimeout(id: Int, ms: Long) { cancelTimeout(); currentTimeout = Runnable { if (id == activeAttemptId && isConnecting) { Log.e(tag, "[BLE][$id] setup timeout state=${_connectionState.value}"); bluetoothGatt?.let { handleError(id, it) } } }; handler.postDelayed(currentTimeout!!, ms) }
    private fun cancelTimeout() { currentTimeout?.let(handler::removeCallbacks); currentTimeout = null }

    @SuppressLint("MissingPermission")
    private fun closeGatt(disconnect: Boolean) {
        val g = bluetoothGatt; bluetoothGatt = null
        if (disconnect) try { g?.disconnect() } catch (_: Exception) {}
        try { g?.close() } catch (_: Exception) {}
        commandChar = null; responseChar = null; controllerStateChar = null; otaControlChar = null; otaDataChar = null; otaStatusChar = null
        descriptorQueue.clear(); synchronized(writeQueue) { writeQueue.clear(); writeInProgress = false }
        _isGamepadServiceReady.value = false; _isCompanionServiceReady.value = false; _isOtaReadyStatus.value = false
        serviceDiscoveryStarted = false; serviceDiscoveryRetried = false; isConnecting = false; cancelTimeout()
    }
    private fun handleDisconnect(id: Int, g: BluetoothGatt) { if (id == activeAttemptId && isActiveGatt(g)) { closeGatt(false); _connectionState.value = ConnectionState.DISCONNECTED; _connectedDeviceName.value = null; resetInputState() } }
    private fun handleError(id: Int, g: BluetoothGatt) { if (id == activeAttemptId && isActiveGatt(g)) { closeGatt(true); _connectionState.value = ConnectionState.ERROR; resetInputState() } }
    private fun resetInputState() { _inputState.value = ControllerInputState(); _deviceInfo.value = DeviceInfo(); _deviceSettings.value = DeviceSettings(); _batteryLevel.value = null; synchronized(responseBuffer) { responseBuffer.setLength(0) }; synchronized(stateBuffer) { stateBuffer.setLength(0) }; synchronized(otaBuffer) { otaBuffer.setLength(0) } }
}
