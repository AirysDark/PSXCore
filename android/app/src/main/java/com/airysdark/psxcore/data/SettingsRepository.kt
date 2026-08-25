package com.airysdark.psxcore.data

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "settings")

class SettingsRepository(private val context: Context) {
    private val LAST_DEVICE_ADDRESS = stringPreferencesKey("last_device_address")
    private val LAST_DEVICE_NAME = stringPreferencesKey("last_device_name")

    val lastDeviceAddress: Flow<String?> = context.dataStore.data.map { it[LAST_DEVICE_ADDRESS] }
    val lastDeviceName: Flow<String?> = context.dataStore.data.map { it[LAST_DEVICE_NAME] }

    suspend fun saveLastDevice(address: String, name: String?) {
        context.dataStore.edit { settings ->
            settings[LAST_DEVICE_ADDRESS] = address
            settings[LAST_DEVICE_NAME] = name ?: "PSXCore"
        }
    }

    suspend fun clearLastDevice() {
        context.dataStore.edit { settings ->
            settings.remove(LAST_DEVICE_ADDRESS)
            settings.remove(LAST_DEVICE_NAME)
        }
    }
}
