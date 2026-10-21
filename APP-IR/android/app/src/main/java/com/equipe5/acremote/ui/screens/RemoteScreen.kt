package com.equipe5.acremote.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.equipe5.acremote.ui.model.AcRemoteUiState
import com.equipe5.acremote.ui.theme.*

@Composable
fun RemoteScreen(uiState: AcRemoteUiState, onOpenMenu: () -> Unit, onDecreaseTemperature: () -> Unit, onIncreaseTemperature: () -> Unit, onTogglePower: () -> Unit) {
    Column(Modifier.fillMaxSize().padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) { IconButton(onClick = onOpenMenu) { Icon(Icons.Default.Menu, "Abrir menu", tint = Ink) } }
        Spacer(Modifier.height(8.dp))
        Text(uiState.brand, color = Ink, fontSize = 26.sp, fontWeight = FontWeight.Bold)
        Text("Controle registrado", color = MutedText, fontSize = 14.sp)
        Spacer(Modifier.height(28.dp))
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(Color.White).padding(28.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(if (uiState.isOn) "LIGADO" else "DESLIGADO", color = if (uiState.isOn) Cyan else Color(0xFF82909C), fontSize = 13.sp, fontWeight = FontWeight.Bold)
            TemperatureDial(uiState.temperature, uiState.isOn)
            Spacer(Modifier.height(16.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
                TemperatureButton("-", uiState.isOn, onDecreaseTemperature)
                PowerButton(uiState.isOn, onTogglePower)
                TemperatureButton("+", uiState.isOn, onIncreaseTemperature)
            }
        }
    }
}

@Composable private fun TemperatureDial(temperature: Int, isOn: Boolean) {
    Box(Modifier.size(230.dp), contentAlignment = Alignment.Center) {
        CircularProgressIndicator(progress = { (temperature - 16) / 14f }, modifier = Modifier.fillMaxSize().padding(10.dp), color = if (isOn) Cyan else Color(0xFFD9E0E5), trackColor = Color(0xFFF0F3F5), strokeWidth = 12.dp, strokeCap = StrokeCap.Round)
        Column(horizontalAlignment = Alignment.CenterHorizontally) { Text("Temperatura", color = Color(0xFF82909C), fontSize = 12.sp); Text("${temperature}°C", color = if (isOn) Ink else Color(0xFF82909C), fontSize = 58.sp, fontWeight = FontWeight.Light) }
    }
}

@Composable private fun TemperatureButton(label: String, enabled: Boolean, onClick: () -> Unit) { Box(Modifier.size(64.dp).clip(CircleShape).background(if (enabled) Cyan else Color(0xFFE4E9EC)).clickable(enabled = enabled, onClick = onClick), contentAlignment = Alignment.Center) { Text(label, color = Color.White, fontSize = 36.sp, textAlign = TextAlign.Center) } }
@Composable private fun PowerButton(isOn: Boolean, onClick: () -> Unit) { Box(Modifier.size(78.dp).clip(CircleShape).background(Orange).clickable(onClick = onClick), contentAlignment = Alignment.Center) { Icon(Icons.Default.PowerSettingsNew, if (isOn) "Desligar" else "Ligar", tint = Color.White, modifier = Modifier.size(34.dp)) } }
