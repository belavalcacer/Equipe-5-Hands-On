package com.equipe5.acremote.ui.model

data class AcRemoteUiState(
    val brand: String = "LG",
    val temperature: Int = 24,
    val isOn: Boolean = true,
    val screen: AppScreen = AppScreen.Remote,
    val deviceName: String = "",
    val deviceBrand: String = "",
    val learningCommand: IrCommand? = null,
    val learningTimedOutCommand: IrCommand? = null,
    val learnedCommands: Set<IrCommand> = emptySet(),
    val controls: List<RegisteredControl> = listOf(
        RegisteredControl(
            id = 1L,
            name = "Controle LG",
            brand = "LG",
            irCommands = IrCommand.entries.toSet()
        )
    ),
    val activeControlId: Long = 1L,
    val nextControlId: Long = 2L
)

data class RegisteredControl(
    val id: Long,
    val name: String,
    val brand: String,
    val irCommands: Set<IrCommand>
)

enum class AppScreen { Remote, AddDevice }

enum class IrCommand {
    PowerOn,
    PowerOff,
    TemperatureUp,
    TemperatureDown
}
