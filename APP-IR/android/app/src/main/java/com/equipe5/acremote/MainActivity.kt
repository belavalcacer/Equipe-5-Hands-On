package com.equipe5.acremote

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AcUnit
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.screens.AddDeviceScreen
import com.equipe5.acremote.ui.screens.NoControlsScreen
import com.equipe5.acremote.ui.screens.RemoteScreen
import com.equipe5.acremote.ui.screens.LoadingScreen
import com.equipe5.acremote.ui.theme.Cyan
import com.equipe5.acremote.ui.theme.Ink
import com.equipe5.acremote.ui.theme.Pale
import com.equipe5.acremote.ui.viewmodel.AcRemoteViewModel
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay

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
    var isLoading by remember { mutableStateOf(true) }
    var pendingDeleteControlId by remember { mutableStateOf<Long?>(null) }

    LaunchedEffect(Unit) {
        delay(1_500)
        isLoading = false
    }

    MaterialTheme {
        if (isLoading) {
            LoadingScreen()
            return@MaterialTheme
        }

        ModalNavigationDrawer(drawerState = drawerState, drawerContent = {
            ModalDrawerSheet {
                Column(Modifier.padding(24.dp)) {
                    Text("Controles por marca", color = Ink, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                    Spacer(Modifier.height(20.dp))
                    uiState.controls.forEach { control ->
                        Row(
                            modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(
                                modifier = Modifier.weight(1f).clip(RoundedCornerShape(14.dp)).clickable {
                                    scope.launch { drawerState.close() }
                                    viewModel.selectControl(control.id)
                                }.padding(vertical = 12.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Icon(Icons.Default.AcUnit, null, tint = if (control.id == uiState.activeControlId) Cyan else Ink)
                                Spacer(Modifier.width(12.dp))
                                Column {
                                    Text(control.brand, color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Medium)
                                    Text(control.name, color = Ink.copy(alpha = 0.62f), fontSize = 12.sp)
                                }
                            }
                            IconButton(onClick = {
                                scope.launch { drawerState.close() }
                                viewModel.editControl(control.id)
                            }) { Icon(Icons.Default.Edit, contentDescription = "Editar ${control.name}", tint = Cyan) }
                            IconButton(onClick = {
                                pendingDeleteControlId = control.id
                            }) { Icon(Icons.Default.Delete, contentDescription = "Excluir ${control.name}", tint = Color(0xFFFF694F)) }
                        }
                    }
                    Spacer(Modifier.height(16.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).clickable {
                            scope.launch { drawerState.close() }
                            viewModel.openAddDevice()
                        }.padding(vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(Icons.Default.Add, null, tint = Cyan)
                        Spacer(Modifier.width(12.dp))
                        Text("Adicionar controle", color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Medium)
                    }
                }
            }
        }) {
            Surface(Modifier.fillMaxSize(), color = Pale) {
                when (uiState.screen) {
                    AppScreen.Remote -> if (uiState.controls.isEmpty()) {
                        NoControlsScreen(onCreateControl = viewModel::openAddDevice)
                    } else {
                        RemoteScreen(uiState, { scope.launch { drawerState.open() } }, viewModel::decreaseTemperature, viewModel::increaseTemperature, viewModel::togglePower, viewModel::setFixedTemperature)
                    }
                    AppScreen.AddDevice -> AddDeviceScreen(uiState, viewModel::closeAddDevice, viewModel::updateDeviceName, viewModel::updateDeviceBrand, viewModel::startLearning, viewModel::confirmIrCommandLearned, viewModel::addFixedTemperature, viewModel::removeFixedTemperature, viewModel::saveDevice)
                }
            }
        }

        pendingDeleteControlId?.let { controlId ->
            val control = uiState.controls.firstOrNull { it.id == controlId }
            if (control != null) {
                AlertDialog(
                    onDismissRequest = { pendingDeleteControlId = null },
                    title = { Text("Excluir controle?") },
                    text = { Text("O controle ${control.name} e os comandos cadastrados serão removidos deste dispositivo.") },
                    confirmButton = {
                        TextButton(onClick = {
                            viewModel.deleteControl(controlId)
                            pendingDeleteControlId = null
                        }) { Text("Excluir", color = Color(0xFFFF694F)) }
                    },
                    dismissButton = { TextButton(onClick = { pendingDeleteControlId = null }) { Text("Cancelar") } }
                )
            }
        }

        uiState.missingCommandMessage?.let { message ->
            AlertDialog(
                onDismissRequest = viewModel::dismissMissingCommandMessage,
                title = { Text("Comando não cadastrado") },
                text = { Text(message) },
                confirmButton = { TextButton(onClick = viewModel::dismissMissingCommandMessage) { Text("Entendi") } }
            )
        }

    }
}
