package com.equipe5.acremote.ui.model

data class AcRemoteUiState(
    val brand: String = "LG",
    val temperature: Int = 24,
    val isOn: Boolean = true,
    val screen: AppScreen = AppScreen.Remote,
    val deviceName: String = "",
    val deviceBrand: String = "",
    val learningCommand: IrCommand? = null,
    val learnedCommands: Set<IrCommand> = emptySet()
)

enum class AppScreen { Remote, AddDevice }

enum class IrCommand {
    PowerOn,
    PowerOff,
    TemperatureUp,
    TemperatureDown
}
