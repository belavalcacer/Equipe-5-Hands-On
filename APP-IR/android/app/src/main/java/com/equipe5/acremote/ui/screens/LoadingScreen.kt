package com.equipe5.acremote.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.equipe5.acremote.R
import com.equipe5.acremote.ui.theme.Cyan
import com.equipe5.acremote.ui.theme.Ink
import com.equipe5.acremote.ui.theme.Pale
import androidx.compose.material3.Surface

@Composable
fun LoadingScreen() {
    Surface(modifier = Modifier.fillMaxSize(), color = Pale) {
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Image(
                painter = painterResource(R.drawable.titan_beam_logo),
                contentDescription = "Titan Beam",
                modifier = Modifier.size(180.dp),
                contentScale = ContentScale.Fit
            )
            Spacer(Modifier.height(24.dp))
            Text("Titan Beam", color = Ink, fontSize = 25.sp, fontWeight = FontWeight.Bold)
            Text("Preparando seu controle", color = Ink.copy(alpha = 0.62f), fontSize = 14.sp)
            Spacer(Modifier.height(28.dp))
            CircularProgressIndicator(color = Cyan)
        }
    }
}
