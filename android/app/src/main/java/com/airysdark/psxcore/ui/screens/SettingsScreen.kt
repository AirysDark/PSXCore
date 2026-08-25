package com.airysdark.psxcore.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.*
import androidx.compose.runtime.*
import android.util.Log
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.airysdark.psxcore.ui.MainViewModel

@Composable
fun SettingsScreen(viewModel: MainViewModel) {
    val scrollState = rememberScrollState()
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(scrollState)
    ) {
        Text("App Settings", fontWeight = FontWeight.Bold, fontSize = 24.sp)
        
        Spacer(modifier = Modifier.height(24.dp))

        SettingsGroup("Display") {
            var darkTheme by remember { mutableStateOf(true) }
            SettingsToggle("Dark Mode", darkTheme) { darkTheme = it }
        }

        Spacer(modifier = Modifier.height(16.dp))

        SettingsGroup("Bluetooth") {
            var autoReconnect by remember { mutableStateOf(true) }
            SettingsToggle("Auto Reconnect", autoReconnect) { autoReconnect = it }
            
            var rememberDevice by remember { mutableStateOf(true) }
            SettingsToggle("Remember Last Controller", rememberDevice) { rememberDevice = it }
        }

        Spacer(modifier = Modifier.height(16.dp))

        SettingsGroup("Debug") {
            var debugLogging by remember { mutableStateOf(false) }
            SettingsToggle("Enable Debug Logging", debugLogging) { debugLogging = it }
            
            Button(
                onClick = { 
                    Log.d("PSXCore", "Button clicked: Clear Saved Data")
                    /* TODO: Clear DataStore */ 
                },
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
            ) {
                Text("Clear Saved Data")
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Icon(Icons.Default.Info, contentDescription = null, tint = Color.Gray, modifier = Modifier.size(16.dp))
            Spacer(modifier = Modifier.width(4.dp))
            Text("PSXCore Companion v0.1.0", color = Color.Gray, fontSize = 12.sp)
        }
    }
}

@Composable
fun SettingsGroup(title: String, content: @Composable ColumnScope.() -> Unit) {
    Column {
        Text(title, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary, fontSize = 14.sp)
        Spacer(modifier = Modifier.height(8.dp))
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp)) {
                content()
            }
        }
    }
}

@Composable
fun SettingsToggle(label: String, checked: Boolean, onCheckedChange: (Boolean) -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(label)
        Switch(checked = checked, onCheckedChange = onCheckedChange)
    }
}
