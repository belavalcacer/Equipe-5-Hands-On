package com.equipe5.acremote.ui.model

data class AcRemoteUiState(
    val brand: String = "",
    val temperature: Int = 24,
    val isOn: Boolean = false,
    val screen: AppScreen = AppScreen.Remote,
    val deviceName: String = "",
    val deviceBrand: String = "",
    val editingControlId: Long? = null,
    val learningCommand: IrCommand? = null,
    val learningTimedOutCommand: IrCommand? = null,
    val learnedCommands: Set<IrCommand> = emptySet(),
    val controls: List<RegisteredControl> = emptyList(),
    val activeControlId: Long = 0L,
    val nextControlId: Long = 1L
)

data class RegisteredControl(
    val id: Long,
    val name: String,
    val brand: String,
    val irCommands: Set<IrCommand>,
    val temperature: Int = 24,
    val isOn: Boolean = true
)

enum class AppScreen { Remote, AddDevice }

enum class IrCommand {
    PowerOn,
    PowerOff,
    TemperatureUp,
    TemperatureDown
}
