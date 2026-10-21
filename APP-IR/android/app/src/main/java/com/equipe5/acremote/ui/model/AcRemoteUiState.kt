package com.equipe5.acremote.ui.model

data class AcRemoteUiState(
    val brand: String = "",
    val temperature: Int = 24,
    val isOn: Boolean = false,
    val screen: AppScreen = AppScreen.Remote,
    val deviceName: String = "",
    val deviceBrand: String = "",
    val editingControlId: Long? = null,
    val learningCommand: LearnableCommand? = null,
    val learningTimedOutCommand: LearnableCommand? = null,
    val learnedCommands: Set<IrCommand> = emptySet(),
    val fixedTemperatures: Set<Int> = emptySet(),
    val learnedFixedTemperatures: Set<Int> = emptySet(),
    val irSignals: Map<String, StoredIrSignal> = emptyMap(),
    val missingCommandMessage: String? = null,
    val learningErrorMessage: String? = null,
    val controls: List<RegisteredControl> = emptyList(),
    val activeControlId: Long = 0L,
    val nextControlId: Long = 1L
)

data class RegisteredControl(
    val id: Long,
    val name: String,
    val brand: String,
    val irCommands: Set<IrCommand>,
    val fixedTemperatureCommands: Set<Int> = emptySet(),
    val irSignals: Map<String, StoredIrSignal> = emptyMap(),
    val temperature: Int = 24,
    val isOn: Boolean = true
)

data class StoredIrSignal(
    val carrierFrequencyHz: Int,
    val pattern: List<Int>
)

enum class AppScreen { Remote, AddDevice }

sealed interface LearnableCommand

enum class IrCommand : LearnableCommand {
    PowerOn,
    PowerOff,
    TemperatureUp,
    TemperatureDown
}

data class FixedTemperatureCommand(val temperature: Int) : LearnableCommand

fun LearnableCommand.signalKey(): String = when (this) {
    is IrCommand -> name
    is FixedTemperatureCommand -> "fixed_$temperature"
}
