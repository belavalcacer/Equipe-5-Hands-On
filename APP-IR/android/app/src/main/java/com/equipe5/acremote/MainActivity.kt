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
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.Icon
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.equipe5.acremote.ui.model.AppScreen
import com.equipe5.acremote.ui.screens.AddDeviceScreen
import com.equipe5.acremote.ui.screens.RemoteScreen
import com.equipe5.acremote.ui.theme.Cyan
import com.equipe5.acremote.ui.theme.Ink
import com.equipe5.acremote.ui.theme.Pale
import com.equipe5.acremote.ui.viewmodel.AcRemoteViewModel
import kotlinx.coroutines.launch

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
        ModalNavigationDrawer(drawerState = drawerState, drawerContent = {
            ModalDrawerSheet {
                Column(Modifier.padding(24.dp)) {
                    Text("Controles por marca", color = Ink, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                    Spacer(Modifier.height(20.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Default.AcUnit, null, tint = Cyan)
                        Spacer(Modifier.width(12.dp))
                        Text(uiState.brand, color = Ink, fontSize = 16.sp, fontWeight = FontWeight.Medium)
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
                    AppScreen.Remote -> RemoteScreen(uiState, { scope.launch { drawerState.open() } }, viewModel::decreaseTemperature, viewModel::increaseTemperature, viewModel::togglePower)
                    AppScreen.AddDevice -> AddDeviceScreen(uiState, viewModel::closeAddDevice, viewModel::updateDeviceName, viewModel::updateDeviceBrand, viewModel::startLearning, viewModel::confirmIrCommandLearned, viewModel::saveDevice)
                }
            }
        }
    }
}
