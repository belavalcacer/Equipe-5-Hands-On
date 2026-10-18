package com.equipe5.acremote.ui.viewmodel

import androidx.lifecycle.ViewModel
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.model.IrCommand
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

class AcRemoteViewModel : ViewModel() {
    private val _uiState = MutableStateFlow(AcRemoteUiState())
    val uiState = _uiState.asStateFlow()

    fun increaseTemperature() = updateState {
        if (it.isOn && it.temperature < MAX_TEMPERATURE) it.copy(temperature = it.temperature + 1) else it
    }

    fun decreaseTemperature() = updateState {
        if (it.isOn && it.temperature > MIN_TEMPERATURE) it.copy(temperature = it.temperature - 1) else it
    }

    fun togglePower() = updateState { it.copy(isOn = !it.isOn) }

    fun openAddDevice() = updateState {
        it.copy(
            screen = AppScreen.AddDevice,
            deviceName = "",
            deviceBrand = "",
            learningCommand = null,
            learnedCommands = emptySet()
        )
    }

    fun closeAddDevice() = updateState { it.copy(screen = AppScreen.Remote, learningCommand = null) }

    fun updateDeviceName(name: String) = updateState { it.copy(deviceName = name) }

    fun updateDeviceBrand(brand: String) = updateState { it.copy(deviceBrand = brand) }

    /**
     * Ponto de integração futuro: o serviço de IR deve iniciar a escuta aqui e
     * chamar [confirmIrCommandLearned] após receber um sinal válido da HAL.
     */
    fun startLearning(command: IrCommand) = updateState { it.copy(learningCommand = command) }

    // Simula a confirmação visual enquanto a captura real ainda não está conectada.
    fun confirmIrCommandLearned(command: IrCommand) = updateState {
        it.copy(learningCommand = null, learnedCommands = it.learnedCommands + command)
    }

    fun saveDevice() = updateState {
        it.copy(
            brand = it.deviceBrand.ifBlank { it.deviceName }.ifBlank { it.brand },
            screen = AppScreen.Remote,
            learningCommand = null
        )
    }

    private fun updateState(transform: (AcRemoteUiState) -> AcRemoteUiState) {
        _uiState.update(transform)
    }

    private companion object {
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
    }
}
