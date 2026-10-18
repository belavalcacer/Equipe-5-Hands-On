package com.equipe5.acremote.ui.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.model.IrCommand
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class AcRemoteViewModel : ViewModel() {
    private val _uiState = MutableStateFlow(AcRemoteUiState())
    val uiState = _uiState.asStateFlow()
    private var learningTimeoutJob: Job? = null

    fun increaseTemperature() = updateState {
        if (it.isOn && it.temperature < MAX_TEMPERATURE) it.copy(temperature = it.temperature + 1) else it
    }

    fun decreaseTemperature() = updateState {
        if (it.isOn && it.temperature > MIN_TEMPERATURE) it.copy(temperature = it.temperature - 1) else it
    }

    fun togglePower() = updateState { it.copy(isOn = !it.isOn) }

    fun openAddDevice() = updateState {
        learningTimeoutJob?.cancel()
        it.copy(
            screen = AppScreen.AddDevice,
            deviceName = "",
            deviceBrand = "",
            learningCommand = null,
            learningTimedOutCommand = null,
            learnedCommands = emptySet()
        )
    }

    fun closeAddDevice() {
        learningTimeoutJob?.cancel()
        updateState { it.copy(screen = AppScreen.Remote, learningCommand = null, learningTimedOutCommand = null) }
    }

    fun updateDeviceName(name: String) = updateState { it.copy(deviceName = name) }

    fun updateDeviceBrand(brand: String) = updateState { it.copy(deviceBrand = brand) }

    /**
     * Ponto de integração futuro: o serviço de IR deve iniciar a escuta aqui e
     * chamar [confirmIrCommandLearned] após receber um sinal válido da HAL.
     */
    fun startLearning(command: IrCommand) {
        learningTimeoutJob?.cancel()
        updateState { it.copy(learningCommand = command, learningTimedOutCommand = null) }
        learningTimeoutJob = viewModelScope.launch {
            delay(IR_LEARNING_TIMEOUT_MS)
            updateState { state ->
                if (state.learningCommand == command) {
                    state.copy(learningCommand = null, learningTimedOutCommand = command)
                } else {
                    state
                }
            }
        }
    }

    // Simula a confirmação visual enquanto a captura real ainda não está conectada.
    fun confirmIrCommandLearned(command: IrCommand) {
        learningTimeoutJob?.cancel()
        updateState {
            it.copy(
                learningCommand = null,
                learningTimedOutCommand = null,
                learnedCommands = it.learnedCommands + command
            )
        }
    }

    fun saveDevice() {
        learningTimeoutJob?.cancel()
        updateState {
            it.copy(
                brand = it.deviceBrand.ifBlank { it.deviceName }.ifBlank { it.brand },
                screen = AppScreen.Remote,
                learningCommand = null,
                learningTimedOutCommand = null
            )
        }
    }

    private fun updateState(transform: (AcRemoteUiState) -> AcRemoteUiState) {
        _uiState.update(transform)
    }

    private companion object {
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
        const val IR_LEARNING_TIMEOUT_MS = 15_000L
    }
}
