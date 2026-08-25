package com.airysdark.psxcore.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import android.util.Log
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.airysdark.psxcore.ui.MainViewModel
import com.airysdark.psxcore.update.UpdateState
import com.airysdark.psxcore.ble.ConnectionState

@Composable
fun FirmwareScreen(viewModel: MainViewModel) {
    val updateState by viewModel.updateManager.updateState.collectAsState()
    val progress by viewModel.updateManager.progress.collectAsState()
    val fileName by viewModel.updateManager.selectedFileName.collectAsState()
    val fileSize by viewModel.updateManager.selectedFileSize.collectAsState()
    val deviceInfo by viewModel.deviceInfo.collectAsState()
    val connectionState by viewModel.bleConnectionManager.connectionState.collectAsState()
    val scrollState = rememberScrollState()

    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        uri?.let { viewModel.onFileSelected(it, "firmware_update.bin", 0) }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(scrollState)
    ) {
        Text("Firmware Update", fontWeight = FontWeight.Bold, fontSize = 24.sp)
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            "Update your PSXCore controller firmware to the latest version.",
            color = Color.Gray,
            fontSize = 14.sp
        )

        Spacer(modifier = Modifier.height(24.dp))

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text("Current Version", fontWeight = FontWeight.Bold)
                Text(if (connectionState == ConnectionState.READY) "${deviceInfo.firmwareVersion} (Detected)" else "Not connected", color = Color.Gray)
                
                Spacer(modifier = Modifier.height(16.dp))
                
                HorizontalDivider()
                
                Spacer(modifier = Modifier.height(16.dp))

                Text("Update File", fontWeight = FontWeight.Bold)
                if (fileName != null) {
                    Text("File: $fileName", fontSize = 14.sp)
                    Text("Size: $fileSize bytes", fontSize = 14.sp, color = Color.Gray)
                } else {
                    Text("No file selected", color = Color.Gray)
                }

                Spacer(modifier = Modifier.height(16.dp))

                Button(
                    onClick = { 
                        Log.d("PSXCore", "Button clicked: Select Firmware File")
                        launcher.launch("*/*") 
                    },
                    modifier = Modifier.fillMaxWidth(),
                    enabled = updateState == UpdateState.IDLE || updateState == UpdateState.FILE_SELECTED || updateState == UpdateState.FAILED
                ) {
                    Text("Select Firmware File")
                }
                
                Spacer(modifier = Modifier.height(8.dp))
                
                Button(
                    onClick = { 
                        Log.d("PSXCore", "Button clicked: Check OTA Capability")
                        viewModel.requestOtaInfo() 
                    },
                    modifier = Modifier.fillMaxWidth(),
                    enabled = connectionState == ConnectionState.READY,
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                ) {
                    Text("Check OTA Capability")
                }
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        if (updateState != UpdateState.IDLE && updateState != UpdateState.FILE_SELECTED) {
            UpdateProgressCard(updateState, progress)
        }

        Spacer(modifier = Modifier.height(24.dp))

        Button(
            onClick = { 
                Log.d("PSXCore", "Button clicked: Start Update")
                viewModel.updateManager.startUpdate() 
            },
            modifier = Modifier.fillMaxWidth(),
            enabled = updateState == UpdateState.FILE_SELECTED && connectionState == ConnectionState.READY,
            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary)
        ) {
            Text("Start Update")
        }
        
        if (updateState == UpdateState.FAILED) {
            Text(
                "Update failed: PSXCore firmware protocol not yet available for transfer.",
                color = Color.Red,
                modifier = Modifier.padding(top = 8.dp),
                fontSize = 12.sp
            )
        }
    }
}

@Composable
fun UpdateProgressCard(state: UpdateState, progress: Float) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Transferring Firmware", fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            val safeProgress = progress.coerceIn(0f, 1f)
            LinearProgressIndicator(
                progress = { safeProgress },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(modifier = Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(state.name, fontSize = 12.sp, color = Color.Gray)
                Text("${(safeProgress * 100).toInt()}%", fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}
