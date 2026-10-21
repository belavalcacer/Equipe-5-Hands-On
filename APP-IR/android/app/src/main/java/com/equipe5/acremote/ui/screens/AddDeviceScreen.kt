package com.equipe5.acremote.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.equipe5.acremote.ui.model.*
import com.equipe5.acremote.ui.theme.*

private const val MAX_FIXED_TEMPERATURES = 6

@Composable
fun AddDeviceScreen(uiState: AcRemoteUiState, onBack: () -> Unit, onDeviceNameChange: (String) -> Unit, onDeviceBrandChange: (String) -> Unit, onStartLearning: (LearnableCommand) -> Unit, onConfirmLearning: (LearnableCommand) -> Unit, onAddFixedTemperature: (Int) -> Unit, onRemoveFixedTemperature: (Int) -> Unit, onSave: () -> Unit) {
    var showTemperatureDialog by remember { mutableStateOf(false) }
    var showTemperatureLimitDialog by remember { mutableStateOf(false) }
    var temperatureText by remember { mutableStateOf("") }
    val temperature = temperatureText.toIntOrNull()
    val canAdd = temperature != null && temperature in 16..30 && temperature !in uiState.fixedTemperatures && uiState.fixedTemperatures.size < MAX_FIXED_TEMPERATURES
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) { IconButton(onClick = onBack) { Icon(Icons.Default.ArrowBack, "Voltar", tint = Ink) }; Spacer(Modifier.width(8.dp)); Column { Text(if (uiState.editingControlId != null) "Editar controle" else "Adicionar controle", color = Ink, fontSize = 24.sp, fontWeight = FontWeight.Bold); Text("Cadastre os comandos que o seu controle possui", color = MutedText, fontSize = 14.sp) } }
        Spacer(Modifier.height(24.dp))
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(Color.White).padding(20.dp)) {
            Text("Dados do dispositivo", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Bold); Spacer(Modifier.height(14.dp))
            DeviceTextField("Nome do controle *", "Ex.: Ar-condicionado da sala", uiState.deviceName, onDeviceNameChange); Spacer(Modifier.height(12.dp)); DeviceTextField("Marca", "Ex.: LG, Samsung, Midea", uiState.deviceBrand, onDeviceBrandChange)
        }
        Spacer(Modifier.height(20.dp)); Text("Ensinar comandos", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        Text("Os comandos são opcionais. Aponte o controle físico e toque no comando correspondente.", color = MutedText, fontSize = 13.sp, modifier = Modifier.padding(top = 4.dp, bottom = 12.dp))
        listOf("Ligar" to IrCommand.PowerOn, "Desligar" to IrCommand.PowerOff, "Aumentar temperatura" to IrCommand.TemperatureUp, "Diminuir temperatura" to IrCommand.TemperatureDown).forEach { (title, command) -> LearnCommandCard(title, command, uiState, onStartLearning, onConfirmLearning); Spacer(Modifier.height(10.dp)) }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) { Text("Temperaturas fixas", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Bold); Text("Máximo de 6 comandos cadastrados", color = MutedText, fontSize = 13.sp) }
            TextButton(onClick = {
                if (uiState.fixedTemperatures.size >= MAX_FIXED_TEMPERATURES) showTemperatureLimitDialog = true
                else showTemperatureDialog = true
            }) { Icon(Icons.Default.Add, null, tint = Cyan); Text("Cadastrar", color = Cyan) }
        }
        uiState.fixedTemperatures.sorted().forEach { fixedTemperature -> Spacer(Modifier.height(10.dp)); FixedTemperatureCard(fixedTemperature, uiState, onStartLearning, onConfirmLearning, onRemoveFixedTemperature) }
        Spacer(Modifier.height(24.dp)); Button(onClick = onSave, enabled = uiState.deviceName.isNotBlank(), modifier = Modifier.fillMaxWidth().height(54.dp), shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(containerColor = Cyan, disabledContainerColor = Color(0xFFD9E0E5))) { Text(if (uiState.editingControlId != null) "Salvar alterações" else "Salvar controle", fontWeight = FontWeight.Bold) }
    }
    if (showTemperatureDialog) AlertDialog(onDismissRequest = { showTemperatureDialog = false }, title = { Text("Cadastrar temperatura fixa") }, text = { Column { Text("Informe uma temperatura entre 16°C e 30°C."); OutlinedTextField(temperatureText, { temperatureText = it.filter(Char::isDigit).take(2) }, label = { Text("Temperatura") }, suffix = { Text("°C") }, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), singleLine = true) } }, confirmButton = { TextButton(onClick = { temperature?.let(onAddFixedTemperature); showTemperatureDialog = false }, enabled = canAdd) { Text("Cadastrar") } }, dismissButton = { TextButton(onClick = { showTemperatureDialog = false }) { Text("Cancelar") } })
    if (showTemperatureLimitDialog) AlertDialog(onDismissRequest = { showTemperatureLimitDialog = false }, title = { Text("Limite de temperaturas atingido") }, text = { Text("Remova uma temperatura cadastrada para adicionar outra.") }, confirmButton = { TextButton(onClick = { showTemperatureLimitDialog = false }) { Text("Entendi") } })
}

@Composable private fun DeviceTextField(label: String, placeholder: String, value: String, onValueChange: (String) -> Unit) = OutlinedTextField(value, onValueChange, Modifier.fillMaxWidth(), label = { Text(label) }, placeholder = { Text(placeholder) }, singleLine = true, keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Words), colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = Cyan, focusedLabelColor = Cyan))

@Composable private fun LearnCommandCard(title: String, command: IrCommand, state: AcRemoteUiState, onStart: (LearnableCommand) -> Unit, onConfirm: (LearnableCommand) -> Unit) = CommandCard(title, command, command in state.learnedCommands, state, onStart, onConfirm, null)

@Composable private fun FixedTemperatureCard(temperature: Int, state: AcRemoteUiState, onStart: (LearnableCommand) -> Unit, onConfirm: (LearnableCommand) -> Unit, onRemove: (Int) -> Unit) = CommandCard("${temperature}°C", FixedTemperatureCommand(temperature), temperature in state.learnedFixedTemperatures, state, onStart, onConfirm) { onRemove(temperature) }

@Composable private fun CommandCard(title: String, command: LearnableCommand, learned: Boolean, state: AcRemoteUiState, onStart: (LearnableCommand) -> Unit, onConfirm: (LearnableCommand) -> Unit, onRemove: (() -> Unit)?) {
    val listening = state.learningCommand == command; val timedOut = state.learningTimedOutCommand == command
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Color.White).clickable(enabled = !listening) { onStart(command) }.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(42.dp).clip(CircleShape).background(if (learned) Cyan else Color(0xFFEAF5F8)), contentAlignment = Alignment.Center) { if (learned && !listening) Icon(Icons.Default.CheckCircle, null, tint = Color.White) else Icon(if (command is IrCommand && (command == IrCommand.PowerOn || command == IrCommand.PowerOff)) Icons.Default.PowerSettingsNew else Icons.Default.AcUnit, null, tint = Cyan) }
        Spacer(Modifier.width(14.dp)); Column(Modifier.weight(1f)) { Text(title, color = Ink, fontSize = 15.sp, fontWeight = FontWeight.Medium); Text(if (listening) "Aguardando sinal... toque para confirmar" else if (timedOut) "Tempo esgotado. Toque para tentar novamente." else if (learned) "Comando salvo. Toque para reaprender." else "Toque para ensinar o comando", color = if (listening || timedOut) Cyan else MutedText, fontSize = 12.sp) }
        if (onRemove != null) IconButton(onClick = onRemove) { Icon(Icons.Default.Delete, "Remover temperatura", tint = Color(0xFFFF694F)) } else Icon(Icons.Default.KeyboardArrowRight, null, tint = Cyan)
    }
}
