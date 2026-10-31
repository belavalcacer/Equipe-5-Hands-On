package com.equipe5.acremote.ui.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.equipe5.acremote.data.ControlStateRepository
import com.equipe5.acremote.data.ConsumerIrGateway
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.model.IrCommand
import com.equipe5.acremote.ui.model.FixedTemperatureCommand
import com.equipe5.acremote.ui.model.LearnableCommand
import com.equipe5.acremote.ui.model.RegisteredControl
import com.equipe5.acremote.ui.model.StoredIrSignal
import com.equipe5.acremote.ui.model.signalKey
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull

class AcRemoteViewModel(application: Application) : AndroidViewModel(application) {
    private val stateRepository = ControlStateRepository(application)
    private val irGateway = ConsumerIrGateway()
    private val _uiState = MutableStateFlow(stateRepository.load(AcRemoteUiState()))
    val uiState = _uiState.asStateFlow()
    private var learningTimeoutJob: Job? = null

    fun increaseTemperature() { sendSignal(IrCommand.TemperatureUp); updateState {
        if (!it.hasCommand(IrCommand.TemperatureUp)) it.withMissingCommand("aumentar temperatura")
        else if (it.isOn && it.temperature < MAX_TEMPERATURE) {
            val temperature = it.temperature + 1
            it.copy(
                temperature = temperature,
                controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                    control.copy(temperature = temperature)
                }
            )
        } else it
    } }

    fun decreaseTemperature() { sendSignal(IrCommand.TemperatureDown); updateState {
        if (!it.hasCommand(IrCommand.TemperatureDown)) it.withMissingCommand("diminuir temperatura")
        else if (it.isOn && it.temperature > MIN_TEMPERATURE) {
            val temperature = it.temperature - 1
            it.copy(
                temperature = temperature,
                controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                    control.copy(temperature = temperature)
                }
            )
        } else it
    } }

    fun togglePower() { sendSignal(if (_uiState.value.isOn) IrCommand.PowerOff else IrCommand.PowerOn); updateState {
        val command = if (it.isOn) IrCommand.PowerOff else IrCommand.PowerOn
        if (!it.hasCommand(command)) it.withMissingCommand(if (it.isOn) "desligar" else "ligar") else {
            val isOn = !it.isOn
            it.copy(
            isOn = isOn,
            controls = it.controls.updateActiveControl(it.activeControlId) { control ->
                control.copy(isOn = isOn)
            }
            )
        }
    } }

    fun setFixedTemperature(temperature: Int) { sendSignal(FixedTemperatureCommand(temperature)); updateState {
        if (!it.isOn) it
        else if (FixedTemperatureCommand(temperature).signalKey() !in it.activeControl()?.irSignals.orEmpty()) {
            it.withMissingCommand("temperatura fixa de ${temperature}°C")
        } else it.copy(
            temperature = temperature,
            controls = it.controls.updateActiveControl(it.activeControlId) { control -> control.copy(temperature = temperature) }
        )
    } }

    fun openAddDevice() = updateState {
        learningTimeoutJob?.cancel()
        it.copy(
            screen = AppScreen.AddDevice,
            deviceName = "",
            deviceBrand = "",
            editingControlId = null,
            learningCommand = null,
            learningTimedOutCommand = null,
            learnedCommands = emptySet(),
            fixedTemperatures = emptySet(),
            learnedFixedTemperatures = emptySet(),
            irSignals = emptyMap(),
            missingCommandMessage = null,
            learningErrorMessage = null
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
                fixedTemperatures = control.fixedTemperatureCommands,
                learnedFixedTemperatures = control.fixedTemperatureCommands,
                irSignals = control.irSignals,
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
    fun startLearning(command: LearnableCommand) {
        learningTimeoutJob?.cancel()
        updateState { it.copy(learningCommand = command, learningTimedOutCommand = null) }
        learningTimeoutJob = viewModelScope.launch {
            val result = withTimeoutOrNull(IR_LEARNING_OPERATION_TIMEOUT_MS) {
                withContext(Dispatchers.IO) { irGateway.learn(IR_LEARNING_TIMEOUT_MS.toInt()) }
            }
            updateState { state ->
                if (state.learningCommand != command) state
                else if (result == null) state.copy(learningCommand = null, learningTimedOutCommand = command)
                else result.fold(
                    onSuccess = { signal ->
                        if (signal == null) state.copy(learningCommand = null, learningTimedOutCommand = command)
                        else state.saveLearned(command, signal)
                    },
                    onFailure = { error -> state.copy(
                        learningCommand = null,
                        learningTimedOutCommand = null,
                        learningErrorMessage = "Não foi possível iniciar a leitura IR: ${error.cause?.message ?: error.message ?: "serviço indisponível"}."
                    ) }
                )
            }
        }
    }

    // Simula a confirmação visual enquanto a captura real ainda não está conectada.
    fun confirmIrCommandLearned(command: LearnableCommand) = startLearning(command)

    fun addFixedTemperature(temperature: Int) = updateState { state ->
        if (temperature in MIN_TEMPERATURE..MAX_TEMPERATURE &&
            temperature !in state.fixedTemperatures &&
            state.fixedTemperatures.size < MAX_FIXED_TEMPERATURES
        ) state.copy(fixedTemperatures = state.fixedTemperatures + temperature) else state
    }

    fun removeFixedTemperature(temperature: Int) = updateState {
        it.copy(
            fixedTemperatures = it.fixedTemperatures - temperature,
            learnedFixedTemperatures = it.learnedFixedTemperatures - temperature,
            learningCommand = if (it.learningCommand == FixedTemperatureCommand(temperature)) null else it.learningCommand,
            learningTimedOutCommand = if (it.learningTimedOutCommand == FixedTemperatureCommand(temperature)) null else it.learningTimedOutCommand
        )
    }

    fun dismissMissingCommandMessage() = updateState { it.copy(missingCommandMessage = null) }
    fun dismissLearningErrorMessage() = updateState { it.copy(learningErrorMessage = null) }

    fun saveDevice() {
        learningTimeoutJob?.cancel()
        updateState {
            val editingControl = it.editingControlId?.let { id -> it.controls.firstOrNull { control -> control.id == id } }
            if (editingControl != null) {
                val updatedControl = editingControl.copy(
                    name = it.deviceName.ifBlank { it.deviceBrand },
                    brand = it.deviceBrand,
                    irCommands = it.learnedCommands,
                    fixedTemperatureCommands = it.learnedFixedTemperatures
                    , irSignals = it.irSignals
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
                    fixedTemperatureCommands = it.learnedFixedTemperatures,
                    irSignals = it.irSignals,
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

    private fun AcRemoteUiState.activeControl() = controls.firstOrNull { it.id == activeControlId }
    private fun AcRemoteUiState.hasCommand(command: IrCommand) = command.signalKey() in activeControl()?.irSignals.orEmpty()
    private fun AcRemoteUiState.withMissingCommand(commandName: String) =
        copy(missingCommandMessage = "Cadastre o comando de $commandName antes de usá-lo.")

    private fun AcRemoteUiState.saveLearned(command: LearnableCommand, signal: StoredIrSignal) = copy(
        learningCommand = null,
        learningTimedOutCommand = null,
        learnedCommands = if (command is IrCommand) learnedCommands + command else learnedCommands,
        learnedFixedTemperatures = if (command is FixedTemperatureCommand) learnedFixedTemperatures + command.temperature else learnedFixedTemperatures,
        irSignals = irSignals + (command.signalKey() to signal)
    )

    private fun sendSignal(command: LearnableCommand) {
        val signal = _uiState.value.activeControl()?.irSignals?.get(command.signalKey()) ?: return
        viewModelScope.launch { withContext(Dispatchers.IO) { irGateway.transmit(signal) } }
    }

    private companion object {
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
        const val DEFAULT_TEMPERATURE = 24
        const val NO_ACTIVE_CONTROL_ID = 0L
        const val IR_LEARNING_TIMEOUT_MS = 10_000L
        const val IR_LEARNING_OPERATION_TIMEOUT_MS = 12_000L
        const val MAX_FIXED_TEMPERATURES = 6
    }
}
