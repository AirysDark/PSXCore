package com.airysdark.psxcore.ui.screens

import android.util.Log
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.airysdark.psxcore.ble.ConnectionState
import com.airysdark.psxcore.model.ControllerInputState
import com.airysdark.psxcore.ui.MainViewModel
import com.airysdark.psxcore.ui.components.Ps2ControllerVisualizer
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun ControllerScreen(
    viewModel: MainViewModel,
    onScanRequest: () -> Unit
) {
    val connectionState by viewModel.bleConnectionManager.connectionState.collectAsState()
    val connectedDeviceName by viewModel.bleConnectionManager.connectedDeviceName.collectAsState()
    val lastDeviceName by viewModel.lastDeviceName.collectAsState()
    val inputState by viewModel.controllerInputState.collectAsState()
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(scrollState),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        ConnectionStatusHeader(
            state = connectionState,
            deviceName = connectedDeviceName ?: lastDeviceName,
            onConnect = { viewModel.reconnect() },
            onDisconnect = { viewModel.disconnect() },
            onScan = onScanRequest
        )

        Spacer(modifier = Modifier.height(24.dp))
        Ps2ControllerVisualizer(inputState = inputState, modifier = Modifier.fillMaxWidth())
        Spacer(modifier = Modifier.height(16.dp))
        DeviceInfoCard(viewModel)
        Spacer(modifier = Modifier.height(16.dp))
        ControlButtons(viewModel, connectionState)
        Spacer(modifier = Modifier.height(16.dp))
        ControllerDiagnostics(inputState, connectionState)
        Spacer(modifier = Modifier.height(16.dp))
        DebugLogCard(viewModel)
    }
}

@Composable
fun DeviceInfoCard(viewModel: MainViewModel) {
    val info by viewModel.deviceInfo.collectAsState()
    val battery by viewModel.batteryLevel.collectAsState()
    val state by viewModel.bleConnectionManager.connectionState.collectAsState()
    val gamepadReady by viewModel.isGamepadServiceReady.collectAsState()
    val companionReady by viewModel.isCompanionServiceReady.collectAsState()
    val otaReady by viewModel.isOtaReadyStatus.collectAsState()

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Device Info", fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            if (state == ConnectionState.READY) {
                Text("Version: ${info.firmwareVersion}", fontSize = 12.sp)
                Text("Hardware: ${info.hardwareRevision}", fontSize = 12.sp)
                Text("Protocol: v${info.protocolVersion}", fontSize = 12.sp)
                Text("Build: ${info.buildDate}", fontSize = 12.sp)
                battery?.let {
                    Text(
                        "Battery: $it%",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        color = if (it < 20) Color.Red else Color.Unspecified
                    )
                }
                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))
                ServiceStatusRow("Gamepad Service", gamepadReady)
                ServiceStatusRow("Companion Service", companionReady)
                ServiceStatusRow("OTA Service", otaReady)
            } else {
                Text("No device information available", color = Color.Gray, fontSize = 12.sp)
            }
        }
    }
}

@Composable
fun ServiceStatusRow(label: String, isReady: Boolean) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(vertical = 2.dp)) {
        Surface(
            modifier = Modifier.size(6.dp),
            shape = MaterialTheme.shapes.small,
            color = if (isReady) Color.Green else Color.Gray
        ) {}
        Spacer(modifier = Modifier.width(8.dp))
        Text(text = "$label: ${if (isReady) "Ready" else "Not Available"}", fontSize = 11.sp)
    }
}

@Composable
fun ControlButtons(viewModel: MainViewModel, state: ConnectionState) {
    var controlsCoolingDown by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val controlsEnabled = state == ConnectionState.READY && !controlsCoolingDown

    fun submit(label: String, action: () -> Boolean) {
        if (!controlsEnabled) return
        Log.d("PSXCore", "Button clicked: $label")
        if (!action()) return

        controlsCoolingDown = true
        scope.launch {
            delay(350)
            controlsCoolingDown = false
        }
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Controls", fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { submit("Ping", viewModel::sendPing) },
                    modifier = Modifier.weight(1f),
                    enabled = controlsEnabled
                ) { Text("Ping") }
                Button(
                    onClick = { submit("Refresh", viewModel::sendGetState) },
                    modifier = Modifier.weight(1f),
                    enabled = controlsEnabled
                ) { Text("Refresh") }
            }
            Spacer(modifier = Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { submit("Info", viewModel::sendGetInfo) },
                    modifier = Modifier.weight(1f),
                    enabled = controlsEnabled
                ) { Text("Info") }
                Button(
                    onClick = { submit("Analog", viewModel::setAnalogMode) },
                    modifier = Modifier.weight(1f),
                    enabled = controlsEnabled
                ) { Text("Analog") }
            }
        }
    }
}

@Composable
fun DebugLogCard(viewModel: MainViewModel) {
    val receivedData by viewModel.bleConnectionManager.receivedData.collectAsState()

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Debug Log", fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(100.dp)
                    .verticalScroll(rememberScrollState())
            ) {
                Text(
                    receivedData,
                    fontSize = 10.sp,
                    color = Color.Gray,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
        }
    }
}

@Composable
fun ConnectionStatusHeader(
    state: ConnectionState,
    deviceName: String?,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    onScan: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .padding(16.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(
                    text = deviceName ?: "No Controller",
                    fontWeight = FontWeight.Bold,
                    fontSize = 20.sp
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    val color = when (state) {
                        ConnectionState.READY -> Color.Green
                        ConnectionState.CONNECTING,
                        ConnectionState.DISCOVERING_SERVICES,
                        ConnectionState.ENABLING_NOTIFICATIONS -> Color.Blue
                        ConnectionState.ERROR,
                        ConnectionState.COMPANION_MISSING -> Color.Red
                        ConnectionState.DISCONNECTED -> Color.Gray
                    }
                    Surface(
                        modifier = Modifier.size(8.dp),
                        shape = MaterialTheme.shapes.small,
                        color = color
                    ) {}
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = when (state) {
                            ConnectionState.READY -> "Connected"
                            ConnectionState.CONNECTING -> "Connecting..."
                            ConnectionState.DISCOVERING_SERVICES -> "Discovering..."
                            ConnectionState.ENABLING_NOTIFICATIONS -> "Initializing..."
                            ConnectionState.ERROR -> "Error"
                            ConnectionState.COMPANION_MISSING -> "Companion Service Missing"
                            ConnectionState.DISCONNECTED -> "Disconnected"
                        },
                        fontSize = 14.sp,
                        color = Color.Gray
                    )
                }
            }

            if (state == ConnectionState.READY || state == ConnectionState.COMPANION_MISSING) {
                Button(onClick = {
                    Log.d("PSXCore", "Button clicked: Disconnect")
                    onDisconnect()
                }) { Text("Disconnect") }
            } else if (state == ConnectionState.DISCONNECTED || state == ConnectionState.ERROR) {
                if (deviceName != null) {
                    Button(onClick = {
                        Log.d("PSXCore", "Button clicked: Reconnect ($deviceName)")
                        onConnect()
                    }) { Text("Reconnect") }
                } else {
                    Button(onClick = {
                        Log.d("PSXCore", "Button clicked: Scan")
                        onScan()
                    }) { Text("Scan") }
                }
            } else {
                CircularProgressIndicator(modifier = Modifier.size(24.dp), strokeWidth = 2.dp)
            }
        }
    }
}

@Composable
fun ControllerDiagnostics(inputState: ControllerInputState, state: ConnectionState) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Diagnostics", fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            if (state == ConnectionState.READY) {
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Column {
                        Text("LX: ${inputState.leftStickX} LY: ${inputState.leftStickY}", fontSize = 12.sp)
                        Text("RX: ${inputState.rightStickX} RY: ${inputState.rightStickY}", fontSize = 12.sp)
                    }
                    Column(horizontalAlignment = Alignment.End) {
                        Text("Packets: ${inputState.packetCount}", fontSize = 12.sp)
                        Text("Analog Mode: ${if (inputState.analogMode) "ON" else "OFF"}", fontSize = 12.sp)
                    }
                }
            } else {
                Text("Waiting for controller connection...", color = Color.Gray, fontSize = 12.sp)
            }
        }
    }
}
