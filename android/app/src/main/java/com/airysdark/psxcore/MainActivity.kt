package com.airysdark.psxcore

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import java.nio.charset.StandardCharsets
import java.util.UUID

class MainActivity : AppCompatActivity() {
    private val nusServiceUuid = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    private val nusTxUuid = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
    private val nusRxUuid = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")

    private lateinit var status: TextView
    private lateinit var log: TextView
    private lateinit var scanButton: Button
    private lateinit var pingButton: Button
    private lateinit var infoButton: Button

    private val handler = Handler(Looper.getMainLooper())
    private val adapter: BluetoothAdapter? get() = BluetoothAdapter.getDefaultAdapter()
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private var scanning = false

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { startScan() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildUi())
        requestBluetoothPermissionsAndScan()
    }

    private fun buildUi(): ScrollView {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(32, 32, 32, 32)
        }
        status = TextView(this).apply { textSize = 18f; text = "PSXCore: not connected" }
        scanButton = Button(this).apply { text = "Scan for PSXCore"; setOnClickListener { requestBluetoothPermissionsAndScan() } }
        pingButton = Button(this).apply { text = "PING"; isEnabled = false; setOnClickListener { sendCommand("PING\n") } }
        infoButton = Button(this).apply { text = "Get Device Info"; isEnabled = false; setOnClickListener { sendCommand("INFO\n") } }
        log = TextView(this).apply { text = "Ready.\n"; textIsSelectable = true }
        root.addView(status)
        root.addView(scanButton)
        root.addView(pingButton)
        root.addView(infoButton)
        root.addView(log)
        return ScrollView(this).apply { addView(root) }
    }

    private fun requestBluetoothPermissionsAndScan() {
        val permissions = if (android.os.Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        if (permissions.any { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }) {
            permissionLauncher.launch(permissions)
        } else startScan()
    }

    private fun startScan() {
        if (scanning) return
        scanner = adapter?.bluetoothLeScanner
        if (scanner == null) { append("Bluetooth LE unavailable"); return }
        scanning = true
        status.text = "Scanning for PSXCore..."
        append("BLE scan started")
        scanner?.startScan(scanCallback)
        handler.postDelayed({ if (scanning) stopScan() }, 10_000)
    }

    private fun stopScan() {
        if (!scanning) return
        scanning = false
        scanner?.stopScan(scanCallback)
        append("BLE scan stopped")
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = try { result.device.name ?: "" } catch (_: SecurityException) { "" }
            if (name.contains("PSXCore", ignoreCase = true)) {
                stopScan()
                status.text = "Connecting to $name..."
                append("Found $name")
                gatt = result.device.connectGatt(this@MainActivity, false, gattCallback)
            }
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, statusCode: Int, newState: Int) {
            runOnUiThread {
                if (newState == BluetoothGatt.STATE_CONNECTED) {
                    status.text = "Connected - discovering services..."
                    append("GATT connected")
                    gatt.discoverServices()
                } else {
                    status.text = "Disconnected"
                    pingButton.isEnabled = false
                    infoButton.isEnabled = false
                    append("GATT disconnected: $statusCode")
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, statusCode: Int) {
            val service: BluetoothGattService? = gatt.getService(nusServiceUuid)
            tx = service?.getCharacteristic(nusTxUuid)
            rx = service?.getCharacteristic(nusRxUuid)
            runOnUiThread {
                if (statusCode == BluetoothGatt.GATT_SUCCESS && tx != null && rx != null) {
                    status.text = "PSXCore connected"
                    pingButton.isEnabled = true
                    infoButton.isEnabled = true
                    append("NUS configuration service ready")
                    gatt.setCharacteristicNotification(tx, true)
                    val descriptor = tx?.getDescriptor(cccdUuid)
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                    }
                } else {
                    status.text = "PSXCore config service not found"
                    append("NUS service unavailable")
                }
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val text = characteristic.value?.toString(StandardCharsets.UTF_8) ?: ""
            runOnUiThread { append("RX: $text") }
        }
    }

    private fun sendCommand(command: String) {
        val characteristic = rx ?: run { append("Not connected"); return }
        characteristic.value = command.toByteArray(StandardCharsets.UTF_8)
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        if (!gatt!!.writeCharacteristic(characteristic)) append("Failed to queue command") else append("TX: ${command.trim()}")
    }

    private fun append(message: String) {
        log.append(message + "\n")
    }

    override fun onDestroy() {
        gatt?.close()
        super.onDestroy()
    }
}
