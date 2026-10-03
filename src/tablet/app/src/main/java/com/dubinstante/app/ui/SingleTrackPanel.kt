package com.dubinstante.app.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun SingleTrackPanel(
    isRecording: Boolean,
    onRecordToggle: () -> Unit,
    hasTake: Boolean,
    takeDurationMs: Long,
    isAuditionPlaying: Boolean,
    onAuditionToggle: () -> Unit,
    onClearTake: () -> Unit,
    onExportClick: () -> Unit,
    volume: Float,
    onVolumeChange: (Float) -> Unit,
    micVolume: Float,
    onMicVolumeChange: (Float) -> Unit,
    rythmoSpeed: Float,
    onSpeedChange: (Float) -> Unit,
    hasVideoLoaded: Boolean,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier
            .fillMaxHeight()
            .padding(start = 8.dp, end = 12.dp, top = 8.dp, bottom = 12.dp),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.4f)),
        border = CardDefaults.outlinedCardBorder()
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "Piste 1 • Studio Mic",
                        fontSize = 17.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Text(
                        text = "1 Micro • 1 Bande Rythmo",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }

                // Status Badge
                val statusText = when {
                    isRecording -> "Recording"
                    isAuditionPlaying -> "Audition"
                    hasTake -> "Take Ready"
                    else -> "Standby"
                }
                val statusBg = when {
                    isRecording -> MaterialTheme.colorScheme.error
                    isAuditionPlaying -> MaterialTheme.colorScheme.tertiary
                    hasTake -> Color(0xFF2E7D32)
                    else -> MaterialTheme.colorScheme.surfaceVariant
                }
                Surface(
                    shape = RoundedCornerShape(12.dp),
                    color = statusBg,
                    contentColor = Color.White
                ) {
                    Text(
                        text = statusText,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                    )
                }
            }

            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

            // Record Button Section
            val infiniteTransition = rememberInfiniteTransition(label = "recordPulse")
            val pulseAlpha by infiniteTransition.animateFloat(
                initialValue = 0.3f,
                targetValue = 0.9f,
                animationSpec = infiniteRepeatable(
                    animation = tween(800, easing = LinearEasing),
                    repeatMode = RepeatMode.Reverse
                ),
                label = "recordPulseAlpha"
            )

            val recordButtonColor by animateColorAsState(
                targetValue = if (isRecording) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary,
                label = "recordBtnColor"
            )

            Button(
                onClick = onRecordToggle,
                enabled = hasVideoLoaded,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(58.dp),
                shape = RoundedCornerShape(12.dp),
                colors = ButtonDefaults.buttonColors(containerColor = recordButtonColor)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.Center
                ) {
                    if (isRecording) {
                        Box(
                            modifier = Modifier
                                .size(14.dp)
                                .clip(RoundedCornerShape(3.dp))
                                .background(Color.White)
                        )
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = "STOP RECORDING",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold
                        )
                    } else {
                        Box(
                            modifier = Modifier
                                .size(14.dp)
                                .clip(CircleShape)
                                .background(Color.Red)
                        )
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = if (hasTake) "RECORD NEW TAKE" else "START RECORDING",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }

            if (!hasVideoLoaded) {
                Text(
                    text = "Open a video to enable recording",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            // Take Management Card (Visible when a take has been recorded)
            AnimatedVisibility(visible = hasTake && !isRecording) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(10.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "Recorded Take",
                                fontSize = 14.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = MaterialTheme.colorScheme.primary
                            )
                            val seconds = takeDurationMs / 1000
                            val millis = (takeDurationMs % 1000) / 100
                            Text(
                                text = "${seconds}.${millis}s",
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Bold,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            // Audition / Preview Button
                            FilledTonalButton(
                                onClick = onAuditionToggle,
                                modifier = Modifier.weight(1f),
                                shape = RoundedCornerShape(8.dp)
                            ) {
                                Text(if (isAuditionPlaying) "Pause Audition" else "Audition Take", fontSize = 12.sp)
                            }

                            // Discard Take Button
                            OutlinedButton(
                                onClick = onClearTake,
                                modifier = Modifier.weight(0.7f),
                                shape = RoundedCornerShape(8.dp)
                            ) {
                                Text("Discard", fontSize = 12.sp, color = MaterialTheme.colorScheme.error)
                            }
                        }

                        // Export Button
                        Button(
                            onClick = onExportClick,
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF1B5E20))
                        ) {
                            Text("Export Dubbed Video", fontSize = 14.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }

            // Audio & Band Controls
            Text(
                text = "Audio & Rythmo Controls",
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold,
                color = MaterialTheme.colorScheme.onSurface
            )

            // Video Volume Slider
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("Original Video Audio", fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text("${(volume * 100).toInt()}%", fontSize = 13.sp, fontWeight = FontWeight.Bold)
                }
                Slider(
                    value = volume,
                    onValueChange = onVolumeChange,
                    valueRange = 0f..1f
                )
            }

            // Mic Volume / Gain Slider
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("Microphone Gain", fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text("${(micVolume * 100).toInt()}%", fontSize = 13.sp, fontWeight = FontWeight.Bold)
                }
                Slider(
                    value = micVolume,
                    onValueChange = onMicVolumeChange,
                    valueRange = 0f..2f
                )
            }

            // Rythmo Band Scroll Speed
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("Rythmo Band Speed", fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text("${rythmoSpeed.toInt()} px/s", fontSize = 13.sp, fontWeight = FontWeight.Bold)
                }
                Slider(
                    value = rythmoSpeed,
                    onValueChange = onSpeedChange,
                    valueRange = 80f..500f
                )
            }
        }
    }
}
