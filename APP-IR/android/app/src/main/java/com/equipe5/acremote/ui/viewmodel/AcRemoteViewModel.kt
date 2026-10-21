package com.equipe5.acremote.ui.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.equipe5.acremote.data.ControlStateRepository
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.model.IrCommand
import com.equipe5.acremote.ui.model.RegisteredControl
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class AcRemoteViewModel(application: Application) : AndroidViewModel(application) {
    private val stateRepository = ControlStateRepository(application)
    private val _uiState = MutableStateFlow(stateRepository.load(AcRemoteUiState()))
    val uiState = _uiState.asStateFlow()
    private var learningTimeoutJob: Job? = null

    fun increaseTemperature() = updateState {
        if (it.isOn && it.temperature < MAX_TEMPERATURE) {
            val temperature = it.temperature + 1
            it.copy(
                temperature = temperature,
                controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                    control.copy(temperature = temperature)
                }
            )
        } else it
    }

    fun decreaseTemperature() = updateState {
        if (it.isOn && it.temperature > MIN_TEMPERATURE) {
            val temperature = it.temperature - 1
            it.copy(
                temperature = temperature,
                controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                    control.copy(temperature = temperature)
                }
            )
        } else it
    }

    fun togglePower() = updateState {
        val isOn = !it.isOn
        it.copy(
            isOn = isOn,
            controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                control.copy(isOn = isOn)
            }
        )
    }

    fun openAddDevice() = updateState {
        learningTimeoutJob?.cancel()
        it.copy(
            screen = AppScreen.AddDevice,
            deviceName = "",
            deviceBrand = "",
            editingControlId = null,
            learningCommand = null,
            learningTimedOutCommand = null,
            learnedCommands = emptySet()
        )
    }

    fun closeAddDevice() {
        learningTimeoutJob?.cancel()
        updateState { it.copy(screen = AppScreen.Remote, editingControlId = null, learningCommand = null, learningTimedOutCommand = null) }
    }

    fun updateDeviceName(name: String) = updateState { it.copy(deviceName = name) }

    fun updateDeviceBrand(brand: String) = updateState { it.copy(deviceBrand = brand) }

    fun selectControl(controlId: Long) = updateState { state ->
        state.controls.firstOrNull { it.id == controlId }?.let { control ->
            state.copy(
                brand = control.brand,
                temperature = control.temperature,
                isOn = control.isOn,
                activeControlId = control.id
            )
        } ?: state
    }

    fun editControl(controlId: Long) {
        learningTimeoutJob?.cancel()
        updateState { state ->
            val control = state.controls.firstOrNull { it.id == controlId } ?: return@updateState state
            state.copy(
                screen = AppScreen.AddDevice,
                deviceName = control.name,
                deviceBrand = control.brand,
                editingControlId = control.id,
                learnedCommands = control.irCommands,
                learningCommand = null,
                learningTimedOutCommand = null
            )
        }
    }

    /** Removes a registered control and selects another one when available. */
    fun deleteControl(controlId: Long): Boolean {
        val current = _uiState.value
        if (current.controls.none { it.id == controlId }) return false

        updateState { state ->
            val remainingControls = state.controls.filterNot { it.id == controlId }
            val removedActiveControl = state.activeControlId == controlId
            val nextActiveControl = if (removedActiveControl) remainingControls.firstOrNull() else null
            state.copy(
                controls = remainingControls,
                activeControlId = if (removedActiveControl) nextActiveControl?.id ?: NO_ACTIVE_CONTROL_ID else state.activeControlId,
                brand = if (removedActiveControl) nextActiveControl?.brand.orEmpty() else state.brand,
                temperature = if (removedActiveControl) nextActiveControl?.temperature ?: DEFAULT_TEMPERATURE else state.temperature,
                isOn = if (removedActiveControl) nextActiveControl?.isOn ?: false else state.isOn,
                screen = if (state.editingControlId == controlId) AppScreen.Remote else state.screen,
                editingControlId = if (state.editingControlId == controlId) null else state.editingControlId,
                learningCommand = if (state.editingControlId == controlId) null else state.learningCommand,
                learningTimedOutCommand = if (state.editingControlId == controlId) null else state.learningTimedOutCommand
            )
        }
        return true
    }

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
            val editingControl = it.editingControlId?.let { id -> it.controls.firstOrNull { control -> control.id == id } }
            if (editingControl != null) {
                val updatedControl = editingControl.copy(
                    name = it.deviceName.ifBlank { it.deviceBrand },
                    brand = it.deviceBrand,
                    irCommands = it.learnedCommands
                )
                it.copy(
                    brand = if (it.activeControlId == updatedControl.id) updatedControl.brand else it.brand,
                    screen = AppScreen.Remote,
                    editingControlId = null,
                    learningCommand = null,
                    learningTimedOutCommand = null,
                    controls = it.controls.map { control -> if (control.id == updatedControl.id) updatedControl else control }
                )
            } else {
                val control = RegisteredControl(
                    id = it.nextControlId,
                    name = it.deviceName.ifBlank { it.deviceBrand },
                    brand = it.deviceBrand,
                    irCommands = it.learnedCommands,
                    temperature = DEFAULT_TEMPERATURE,
                    isOn = true
                )
                it.copy(
                    brand = control.brand,
                    temperature = control.temperature,
                    isOn = control.isOn,
                    screen = AppScreen.Remote,
                    editingControlId = null,
                    learningCommand = null,
                    learningTimedOutCommand = null,
                    controls = it.controls + control,
                    activeControlId = control.id,
                    nextControlId = control.id + 1
                )
            }
        }
    }

    private fun updateState(transform: (AcRemoteUiState) -> AcRemoteUiState) {
        _uiState.update { currentState ->
            transform(currentState).also(stateRepository::save)
        }
    }

    private inline fun List<RegisteredControl>.updateActiveControl(
        activeControlId: Long,
        transform: (RegisteredControl) -> RegisteredControl
    ): List<RegisteredControl> = map { control ->
        if (control.id == activeControlId) transform(control) else control
    }

    private companion object {
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
        const val DEFAULT_TEMPERATURE = 24
        const val NO_ACTIVE_CONTROL_ID = 0L
        const val IR_LEARNING_TIMEOUT_MS = 15_000L
    }
}
