package com.equipe5.acremote.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AcUnit
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.equipe5.acremote.ui.theme.Cyan
import com.equipe5.acremote.ui.theme.Ink
import com.equipe5.acremote.ui.theme.MutedText

@Composable
fun NoControlsScreen(onCreateControl: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Icon(Icons.Default.AcUnit, contentDescription = null, tint = Cyan, modifier = Modifier.size(56.dp))
        Spacer(Modifier.height(20.dp))
        Text("Nenhum controle cadastrado", color = Ink, fontSize = 22.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(8.dp))
        Text(
            "Adicione seu primeiro controle para usar o Titan Beam.",
            color = MutedText,
            fontSize = 15.sp,
            textAlign = TextAlign.Center
        )
        Spacer(Modifier.height(24.dp))
        Button(onClick = onCreateControl, shape = RoundedCornerShape(16.dp)) {
            Text("Criar controle", fontWeight = FontWeight.Bold)
        }
    }
}
