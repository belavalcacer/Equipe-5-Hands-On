package com.equipe5.acremote.ui.viewmodel

import androidx.lifecycle.ViewModel
import com.equipe5.acremote.ui.model.AcRemoteUiState
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

    private fun updateState(transform: (AcRemoteUiState) -> AcRemoteUiState) {
        _uiState.update(transform)
    }

    private companion object {
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
    }
}
