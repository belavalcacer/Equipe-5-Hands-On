package com.equipe5.acremote

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AcUnit
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.equipe5.acremote.ui.viewmodel.AcRemoteViewModel
import kotlinx.coroutines.launch

private val Ink = Color(0xFF172A3A)
private val Cyan = Color(0xFF22A9D0)
private val Pale = Color(0xFFF5F8FC)
private val Orange = Color(0xFFFF694F)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { ACRemoteApp() }
    }
}

@Composable
private fun ACRemoteApp(viewModel: AcRemoteViewModel = viewModel()) {
    val uiState = viewModel.uiState.collectAsStateWithLifecycle().value
    val drawerState = rememberDrawerState(DrawerValue.Closed)
    val scope = rememberCoroutineScope()

    MaterialTheme {
        ModalNavigationDrawer(
            drawerState = drawerState,
            drawerContent = {
                ModalDrawerSheet {
                    Column(Modifier.padding(24.dp)) {
                        Text("Controles por marca", color = Ink, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                        Spacer(Modifier.height(20.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(Icons.Default.AcUnit, contentDescription = null, tint = Cyan)
                            Spacer(Modifier.width(12.dp))
                            Text(uiState.brand, color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Medium)
                        }
                    }
                }
            }
        ) {
            Surface(modifier = Modifier.fillMaxSize(), color = Pale) {
                Column(
                    modifier = Modifier.fillMaxSize().padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        IconButton(onClick = { scope.launch { drawerState.open() } }) {
                            Icon(Icons.Default.Menu, contentDescription = "Abrir menu", tint = Ink)
                        }
                    }
                    Spacer(Modifier.height(8.dp))
                    Text("${uiState.brand}", color = Ink, fontSize = 26.sp, fontWeight = FontWeight.Bold)
                    Text("Controle registrado", color = Color(0xFF6D7B87), fontSize = 14.sp)
                    Spacer(Modifier.height(28.dp))

                    Column(
                        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(Color.White).padding(28.dp),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Text(
                            text = if (uiState.isOn) "LIGADO" else "DESLIGADO",
                            color = if (uiState.isOn) Cyan else Color(0xFF82909C),
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Bold
                        )
                        TemperatureDial(uiState.temperature, uiState.isOn)
                        Spacer(Modifier.height(16.dp))
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceEvenly,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            TemperatureButton("-", uiState.isOn, viewModel::decreaseTemperature)
                            PowerButton(uiState.isOn, viewModel::togglePower)
                            TemperatureButton("+", uiState.isOn, viewModel::increaseTemperature)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun TemperatureDial(temperature: Int, isOn: Boolean) {
    Box(modifier = Modifier.size(230.dp), contentAlignment = Alignment.Center) {
        CircularProgressIndicator(
            progress = { (temperature - 16) / 14f },
            modifier = Modifier.fillMaxSize().padding(10.dp),
            color = if (isOn) Cyan else Color(0xFFD9E0E5),
            trackColor = Color(0xFFF0F3F5),
            strokeWidth = 12.dp,
            strokeCap = StrokeCap.Round
        )
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text("Temperatura", color = Color(0xFF82909C), fontSize = 12.sp)
            Text("${temperature}\u00B0C", color = if (isOn) Ink else Color(0xFF82909C), fontSize = 58.sp, fontWeight = FontWeight.Light)
        }
    }
}

@Composable
private fun TemperatureButton(label: String, enabled: Boolean, onClick: () -> Unit) {
    Box(
        modifier = Modifier.size(64.dp).clip(CircleShape)
            .background(if (enabled) Cyan else Color(0xFFE4E9EC))
            .clickable(enabled = enabled, onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(label, color = Color.White, fontSize = 36.sp, textAlign = TextAlign.Center)
    }
}

@Composable
private fun PowerButton(isOn: Boolean, onClick: () -> Unit) {
    Box(
        modifier = Modifier.size(78.dp).clip(CircleShape).background(Orange).clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Icon(
            imageVector = Icons.Default.PowerSettingsNew,
            contentDescription = if (isOn) "Desligar" else "Ligar",
            tint = Color.White,
            modifier = Modifier.size(34.dp)
        )
    }
}
