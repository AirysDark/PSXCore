package com.airysdark.psxcore.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import android.util.Log
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.airysdark.psxcore.ui.MainViewModel
import com.airysdark.psxcore.ble.ConnectionState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfigureScreen(viewModel: MainViewModel) {
    val connectionState by viewModel.bleConnectionManager.connectionState.collectAsState()
    val settings by viewModel.deviceSettings.collectAsState()
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(scrollState)
    ) {
        Text("Controller Configuration", fontWeight = FontWeight.Bold, fontSize = 24.sp)
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            "Configure your PSXCore controller settings. Changes are sent directly to the firmware.",
            color = Color.Gray,
            fontSize = 14.sp
        )
        
        Spacer(modifier = Modifier.height(24.dp))

        if (connectionState != ConnectionState.READY) {
            Card(
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    "You must be connected to a controller to change settings.",
                    modifier = Modifier.padding(16.dp),
                    color = MaterialTheme.colorScheme.onErrorContainer
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
        }

        ConfigurationSection("General Settings") {
            var controllerName by remember(settings.controllerName) { mutableStateOf(settings.controllerName) }
            OutlinedTextField(
                value = controllerName,
                onValueChange = { controllerName = it },
                label = { Text("Controller Name") },
                modifier = Modifier.fillMaxWidth(),
                enabled = connectionState == ConnectionState.READY
            )
            
            Spacer(modifier = Modifier.height(16.dp))
            
            var sleepTimeout by remember(settings.sleepTimeoutMinutes) { mutableFloatStateOf(settings.sleepTimeoutMinutes.toFloat()) }
            Text("Sleep Timeout: ${sleepTimeout.toInt()} minutes")
            Slider(
                value = sleepTimeout,
                onValueChange = { sleepTimeout = it },
                valueRange = 1f..30f,
                steps = 29,
                enabled = connectionState == ConnectionState.READY
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        ConfigurationSection("Analog Mode") {
            var analogModeBehavior by remember(settings.analogModeBehavior) { mutableIntStateOf(settings.analogModeBehavior) }
            val behaviors = listOf("Manual", "Auto-On (Start)", "Persistent")
            
            behaviors.forEachIndexed { index, behavior ->
                Row(modifier = Modifier.fillMaxWidth()) {
                    RadioButton(
                        selected = analogModeBehavior == index,
                        onClick = { analogModeBehavior = index },
                        enabled = connectionState == ConnectionState.READY
                    )
                    Text(behavior, modifier = Modifier.padding(start = 8.dp, top = 12.dp))
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        ConfigurationSection("Advanced") {
            Text("Protocol features not available until firmware protocol is connected", color = Color.Gray, fontSize = 12.sp)
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = { 
                Log.d("PSXCore", "Button clicked: Calibration Tool")
            }, enabled = false, modifier = Modifier.fillMaxWidth()) {
                Text("Calibration Tool")
            }
            Button(onClick = { 
                Log.d("PSXCore", "Button clicked: Factory Reset")
            }, enabled = false, modifier = Modifier.fillMaxWidth()) {
                Text("Factory Reset")
            }
        }
    }
}

@Composable
fun ConfigurationSection(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(title, fontWeight = FontWeight.Bold, fontSize = 18.sp)
            Spacer(modifier = Modifier.height(16.dp))
            content()
        }
    }
}
