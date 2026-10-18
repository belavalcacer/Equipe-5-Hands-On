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
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.model.IrCommand
import com.equipe5.acremote.ui.theme.*

@Composable
fun AddDeviceScreen(uiState: AcRemoteUiState, onBack: () -> Unit, onDeviceNameChange: (String) -> Unit, onDeviceBrandChange: (String) -> Unit, onStartLearning: (IrCommand) -> Unit, onConfirmLearning: (IrCommand) -> Unit, onSave: () -> Unit) {
    val complete = IrCommand.entries.all { it in uiState.learnedCommands }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) { IconButton(onClick = onBack) { Icon(Icons.Default.ArrowBack, "Voltar", tint = Ink) }; Spacer(Modifier.width(8.dp)); Column { Text("Adicionar controle", color = Ink, fontSize = 24.sp, fontWeight = FontWeight.Bold); Text("Cadastre e ensine os comandos IR", color = MutedText, fontSize = 14.sp) } }
        Spacer(Modifier.height(24.dp))
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(Color.White).padding(20.dp)) {
            Text("Dados do dispositivo", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Bold); Spacer(Modifier.height(14.dp))
            DeviceTextField("Nome do controle", "Ex.: Ar-condicionado da sala", uiState.deviceName, onDeviceNameChange); Spacer(Modifier.height(12.dp))
            DeviceTextField("Marca", "Ex.: LG, Samsung, Midea", uiState.deviceBrand, onDeviceBrandChange)
        }
        Spacer(Modifier.height(20.dp)); Text("Ensinar comandos", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        Text("Aponte o controle físico para o dispositivo e toque no comando correspondente.", color = MutedText, fontSize = 13.sp, modifier = Modifier.padding(top = 4.dp, bottom = 12.dp))
        LearnCommandCard("Ligar", "Pressione o botão de ligar no controle físico", IrCommand.PowerOn, uiState, onStartLearning, onConfirmLearning); Spacer(Modifier.height(10.dp))
        LearnCommandCard("Desligar", "Pressione o botão de desligar no controle físico", IrCommand.PowerOff, uiState, onStartLearning, onConfirmLearning); Spacer(Modifier.height(10.dp))
        LearnCommandCard("Aumentar temperatura", "Pressione o botão + no controle físico", IrCommand.TemperatureUp, uiState, onStartLearning, onConfirmLearning); Spacer(Modifier.height(10.dp))
        LearnCommandCard("Diminuir temperatura", "Pressione o botão − no controle físico", IrCommand.TemperatureDown, uiState, onStartLearning, onConfirmLearning)
        Spacer(Modifier.height(24.dp)); Button(onClick = onSave, enabled = uiState.deviceBrand.isNotBlank() && complete, modifier = Modifier.fillMaxWidth().height(54.dp), shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(containerColor = Cyan, disabledContainerColor = Color(0xFFD9E0E5))) { Text("Salvar controle", fontWeight = FontWeight.Bold) }
    }
}

@Composable private fun DeviceTextField(label: String, placeholder: String, value: String, onValueChange: (String) -> Unit) { OutlinedTextField(value, onValueChange, Modifier.fillMaxWidth(), label = { Text(label) }, placeholder = { Text(placeholder) }, singleLine = true, keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Words), colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = Cyan, focusedLabelColor = Cyan)) }
@Composable private fun LearnCommandCard(title: String, hint: String, command: IrCommand, uiState: AcRemoteUiState, onStartLearning: (IrCommand) -> Unit, onConfirmLearning: (IrCommand) -> Unit) {
    val learned = command in uiState.learnedCommands; val listening = uiState.learningCommand == command
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Color.White).clickable(enabled = !learned) { if (listening) onConfirmLearning(command) else onStartLearning(command) }.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(42.dp).clip(CircleShape).background(if (learned) Cyan else Color(0xFFEAF5F8)), contentAlignment = Alignment.Center) { if (learned) Icon(Icons.Default.CheckCircle, null, tint = Color.White) else Icon(if (command == IrCommand.PowerOn || command == IrCommand.PowerOff) Icons.Default.PowerSettingsNew else Icons.Default.AcUnit, null, tint = Cyan) }
        Spacer(Modifier.width(14.dp)); Column(Modifier.weight(1f)) { Text(title, color = Ink, fontSize = 15.sp, fontWeight = FontWeight.Medium); Text(if (listening) "Aguardando sinal do controle... toque para confirmar" else if (learned) "Comando salvo" else hint, color = if (listening) Cyan else MutedText, fontSize = 12.sp) }; if (!learned) Icon(Icons.Default.KeyboardArrowRight, null, tint = Cyan)
    }
}
