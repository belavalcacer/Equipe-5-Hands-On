package com.equipe5.acremote.data

import android.content.Context
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.RegisteredControl
import com.equipe5.acremote.ui.model.IrCommand
import com.equipe5.acremote.ui.model.StoredIrSignal
import org.json.JSONArray
import org.json.JSONObject

/** Stores registered IR controls and the selected remote state on this device. */
class ControlStateRepository(context: Context) {
    private val preferences = context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)

    fun load(defaultState: AcRemoteUiState): AcRemoteUiState {
        val json = preferences.getString(STATE_KEY, null) ?: return defaultState

        return runCatching {
            val root = JSONObject(json)
            val savedActiveControlId = root.optLong(ACTIVE_CONTROL_ID_KEY, defaultState.activeControlId)
            val savedTemperature = root.optInt(TEMPERATURE_KEY, defaultState.temperature)
            val savedIsOn = root.optBoolean(IS_ON_KEY, defaultState.isOn)
            val savedControls = root.getJSONArray(CONTROLS_KEY).toRegisteredControls(
                legacyActiveControlId = savedActiveControlId,
                legacyTemperature = savedTemperature,
                legacyIsOn = savedIsOn
            )
            val controls = savedControls
            val activeControlId = root.optLong(ACTIVE_CONTROL_ID_KEY, controls.firstOrNull()?.id ?: 0L)
                .takeIf { id -> controls.any { it.id == id } }
                ?: controls.firstOrNull()?.id
                ?: 0L
            val activeControl = controls.firstOrNull { it.id == activeControlId }

            defaultState.copy(
                brand = activeControl?.brand.orEmpty(),
                temperature = activeControl?.temperature?.coerceIn(MIN_TEMPERATURE, MAX_TEMPERATURE)
                    ?: defaultState.temperature,
                isOn = activeControl?.isOn ?: false,
                controls = controls,
                activeControlId = activeControlId,
                nextControlId = maxOf(root.optLong(NEXT_CONTROL_ID_KEY, defaultState.nextControlId), (controls.maxOfOrNull { it.id } ?: 0L) + 1L)
            )
        }.getOrElse { defaultState }
    }

    fun save(state: AcRemoteUiState) {
        val controlsJson = JSONArray().apply {
            state.controls.forEach { control ->
                put(JSONObject().apply {
                    put(ID_KEY, control.id)
                    put(NAME_KEY, control.name)
                    put(BRAND_KEY, control.brand)
                    put(TEMPERATURE_KEY, control.temperature)
                    put(IS_ON_KEY, control.isOn)
                    put(COMMANDS_KEY, JSONArray().apply {
                        control.irCommands.forEach { command -> put(command.name) }
                    })
                    put(FIXED_TEMPERATURES_KEY, JSONArray().apply {
                        control.fixedTemperatureCommands.sorted().forEach(::put)
                    })
                    put(SIGNALS_KEY, JSONObject().apply {
                        control.irSignals.forEach { (key, signal) ->
                            put(key, JSONObject().apply {
                                put(CARRIER_FREQUENCY_KEY, signal.carrierFrequencyHz)
                                put(PATTERN_KEY, JSONArray(signal.pattern))
                            })
                        }
                    })
                })
            }
        }

        val stateJson = JSONObject().apply {
            put(CONTROLS_KEY, controlsJson)
            put(ACTIVE_CONTROL_ID_KEY, state.activeControlId)
            put(NEXT_CONTROL_ID_KEY, state.nextControlId)
            put(TEMPERATURE_KEY, state.temperature)
            put(IS_ON_KEY, state.isOn)
        }

        preferences.edit().putString(STATE_KEY, stateJson.toString()).apply()
    }

    private fun JSONArray.toRegisteredControls(
        legacyActiveControlId: Long,
        legacyTemperature: Int,
        legacyIsOn: Boolean
    ): List<RegisteredControl> = buildList {
        for (index in 0 until length()) {
            val item = optJSONObject(index) ?: continue
            val commandsJson = item.optJSONArray(COMMANDS_KEY) ?: JSONArray()
            val commands = buildSet {
                for (commandIndex in 0 until commandsJson.length()) {
                    runCatching { IrCommand.valueOf(commandsJson.optString(commandIndex)) }
                        .getOrNull()?.let(::add)
                }
            }
            val fixedTemperaturesJson = item.optJSONArray(FIXED_TEMPERATURES_KEY) ?: JSONArray()
            val fixedTemperatures = buildSet {
                for (temperatureIndex in 0 until fixedTemperaturesJson.length()) {
                    fixedTemperaturesJson.optInt(temperatureIndex).takeIf {
                        it in MIN_TEMPERATURE..MAX_TEMPERATURE
                    }?.let(::add)
                }
            }.take(MAX_FIXED_TEMPERATURES).toSet()
            val signalsJson = item.optJSONObject(SIGNALS_KEY) ?: JSONObject()
            val signals = buildMap {
                signalsJson.keys().forEach { key ->
                    val signal = signalsJson.optJSONObject(key) ?: return@forEach
                    val pattern = signal.optJSONArray(PATTERN_KEY) ?: return@forEach
                    val durations = List(pattern.length()) { pattern.optInt(it) }
                    if (durations.isNotEmpty() && durations.all { it >= 0 }) {
                        put(key, StoredIrSignal(signal.optInt(CARRIER_FREQUENCY_KEY, DEFAULT_CARRIER_FREQUENCY), durations))
                    }
                }
            }
            add(
                RegisteredControl(
                    id = item.optLong(ID_KEY, index.toLong() + 1L),
                    name = item.optString(NAME_KEY),
                    brand = item.optString(BRAND_KEY),
                    irCommands = commands,
                    fixedTemperatureCommands = fixedTemperatures,
                    irSignals = signals,
                    temperature = item.optInt(
                        TEMPERATURE_KEY,
                        if (item.optLong(ID_KEY) == legacyActiveControlId) legacyTemperature else DEFAULT_TEMPERATURE
                    ).coerceIn(MIN_TEMPERATURE, MAX_TEMPERATURE),
                    isOn = item.optBoolean(
                        IS_ON_KEY,
                        if (item.optLong(ID_KEY) == legacyActiveControlId) legacyIsOn else DEFAULT_IS_ON
                    )
                )
            )
        }
    }

    private companion object {
        const val PREFERENCES_NAME = "titan_beam_control_state"
        const val STATE_KEY = "state"
        const val CONTROLS_KEY = "controls"
        const val ACTIVE_CONTROL_ID_KEY = "activeControlId"
        const val NEXT_CONTROL_ID_KEY = "nextControlId"
        const val TEMPERATURE_KEY = "temperature"
        const val IS_ON_KEY = "isOn"
        const val ID_KEY = "id"
        const val NAME_KEY = "name"
        const val BRAND_KEY = "brand"
        const val COMMANDS_KEY = "commands"
        const val FIXED_TEMPERATURES_KEY = "fixedTemperatures"
        const val SIGNALS_KEY = "signals"
        const val CARRIER_FREQUENCY_KEY = "carrierFrequencyHz"
        const val PATTERN_KEY = "pattern"
        const val DEFAULT_CARRIER_FREQUENCY = 38_000
        const val DEFAULT_TEMPERATURE = 24
        const val DEFAULT_IS_ON = true
        const val MIN_TEMPERATURE = 16
        const val MAX_TEMPERATURE = 30
        const val MAX_FIXED_TEMPERATURES = 6
    }
}
