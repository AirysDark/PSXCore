package com.airysdark.psxcore.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import android.util.Log
import androidx.compose.ui.unit.dp
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.airysdark.psxcore.model.BleDevice
import com.airysdark.psxcore.ui.screens.*
import androidx.compose.foundation.clickable
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items

sealed class Screen(val route: String, val title: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    object Controller : Screen("controller", "Controller", Icons.Default.Gamepad)
    object Configure : Screen("configure", "Configure", Icons.Default.Settings)
    object Firmware : Screen("firmware", "Firmware", Icons.Default.SystemUpdate)
    object Settings : Screen("settings", "Settings", Icons.Default.Tune)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(viewModel: MainViewModel) {
    val navController = rememberNavController()
    var showScanDialog by remember { mutableStateOf(false) }
    
    val items = listOf(
        Screen.Controller,
        Screen.Configure,
        Screen.Firmware,
        Screen.Settings
    )

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("PSXCore Companion") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.primaryContainer,
                    titleContentColor = MaterialTheme.colorScheme.primary
                )
            )
        },
        bottomBar = {
            NavigationBar {
                val navBackStackEntry by navController.currentBackStackEntryAsState()
                val currentRoute = navBackStackEntry?.destination?.route
                
                items.forEach { screen ->
                    NavigationBarItem(
                        icon = { Icon(screen.icon, contentDescription = null) },
                        label = { Text(screen.title) },
                        selected = currentRoute == screen.route,
                        onClick = {
                            Log.d("PSXCore", "Navigation requested: ${screen.route}")
                            navController.navigate(screen.route) {
                                popUpTo(navController.graph.startDestinationId) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = Screen.Controller.route,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(Screen.Controller.route) {
                ControllerScreen(
                    viewModel = viewModel,
                    onScanRequest = { showScanDialog = true }
                )
            }
            composable(Screen.Configure.route) {
                ConfigureScreen(viewModel = viewModel)
            }
            composable(Screen.Firmware.route) {
                FirmwareScreen(viewModel = viewModel)
            }
            composable(Screen.Settings.route) {
                SettingsScreen(viewModel = viewModel)
            }
        }
    }

    if (showScanDialog) {
        ScanDialog(
            viewModel = viewModel,
            onDismiss = { showScanDialog = false },
            onDeviceSelected = { device ->
                viewModel.connectToDevice(device)
                showScanDialog = false
            }
        )
    }
}

@Composable
fun ScanDialog(
    viewModel: MainViewModel,
    onDismiss: () -> Unit,
    onDeviceSelected: (BleDevice) -> Unit
) {
    val devices by viewModel.bleScanner.foundDevices.collectAsState()
    val isScanning by viewModel.bleScanner.isScanning.collectAsState()

    LaunchedEffect(Unit) {
        viewModel.startScan(com.airysdark.psxcore.protocol.ProtocolConstants.PSXCORE_SERVICE_UUID)
    }

    AlertDialog(
        onDismissRequest = {
            viewModel.stopScan()
            onDismiss()
        },
        title = { Text("Scan for Controllers") },
        text = {
            Column(modifier = Modifier.height(300.dp)) {
                if (isScanning) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    Spacer(modifier = Modifier.height(8.dp))
                }
                
                LazyColumn {
                    items(devices) { device ->
                        ListItem(
                            headlineContent = { Text(device.displayName) },
                            supportingContent = { Text(device.address) },
                            trailingContent = { Text("${device.rssi} dBm") },
                            modifier = Modifier.clickable { onDeviceSelected(device) }
                        )
                    }
                }
                
                if (devices.isEmpty() && !isScanning) {
                    Text("No devices found")
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                viewModel.stopScan()
                onDismiss()
            }) { Text("Cancel") }
        }
    )
}
