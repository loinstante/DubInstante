package com.dubinstante.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.dubinstante.app.FontFamilyType
import com.dubinstante.app.RythmoBand
import com.dubinstante.app.RythmoPresets
import com.dubinstante.app.RythmoStyle
import kotlinx.coroutines.delay

@Composable
fun StyleCustomizationDialog(
    initialStyle: RythmoStyle,
    onDismissRequest: () -> Unit,
    onSaveStyle: (RythmoStyle) -> Unit
) {
    var draftStyle by remember { mutableStateOf(initialStyle) }

    // Animated position for live preview
    var previewPositionMs by remember { mutableStateOf(0L) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(16)
            previewPositionMs = (previewPositionMs + 16) % 6000
        }
    }

    Dialog(
        onDismissRequest = onDismissRequest,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth(0.75f)
                .fillMaxHeight(0.85f)
                .padding(16.dp),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(24.dp)
            ) {
                // Header
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "Rythmo Band Customization",
                            fontSize = 22.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Text(
                            text = "Live visual settings for the tablet dubbing band",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    IconButton(onClick = onDismissRequest) {
                        Text("✕", fontSize = 20.sp, color = MaterialTheme.colorScheme.onSurface)
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Live Preview Card
                Text(
                    text = "Live Preview",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.height(6.dp))
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(84.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(8.dp))
                ) {
                    RythmoBand(
                        text = "DubInstante - Bande Rythmo Studio Preview...",
                        onTextChanged = {},
                        currentPositionMs = previewPositionMs,
                        speedPixelsPerSecond = 140,
                        onSeekRequested = {},
                        style = draftStyle,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Scrollable Controls
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .verticalScroll(rememberScrollState())
                ) {
                    // Presets
                    Text(
                        text = "Presets",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        RythmoPresets.all.forEach { (name, preset) ->
                            OutlinedButton(
                                onClick = {
                                    draftStyle = preset.copy(
                                        textSizeSp = draftStyle.textSizeSp,
                                        fontFamilyType = draftStyle.fontFamilyType
                                    )
                                },
                                shape = RoundedCornerShape(8.dp),
                                contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                            ) {
                                Box(
                                    modifier = Modifier
                                        .size(12.dp)
                                        .clip(CircleShape)
                                        .background(preset.textColor)
                                        .border(1.dp, Color.Gray, CircleShape)
                                )
                                Spacer(modifier = Modifier.width(6.dp))
                                Text(name, fontSize = 13.sp)
                            }
                        }
                    }

                    Spacer(modifier = Modifier.height(18.dp))

                    // Text Size Slider
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Text Size",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Text(
                            text = "${draftStyle.textSizeSp.toInt()} sp",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Slider(
                        value = draftStyle.textSizeSp,
                        onValueChange = { draftStyle = draftStyle.copy(textSizeSp = it) },
                        valueRange = 28f..64f,
                        steps = 18
                    )

                    Spacer(modifier = Modifier.height(14.dp))

                    // Font Family Selector
                    Text(
                        text = "Font Family",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Spacer(modifier = Modifier.height(6.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        FontFamilyType.entries.forEach { fontType ->
                            val isSelected = draftStyle.fontFamilyType == fontType
                            FilterChip(
                                selected = isSelected,
                                onClick = { draftStyle = draftStyle.copy(fontFamilyType = fontType) },
                                label = { Text(fontType.displayName) }
                            )
                        }
                    }

                    Spacer(modifier = Modifier.height(18.dp))

                    // Color Palettes
                    val textColors = listOf(
                        Color.White,
                        Color(0xFFE0E0E0),
                        Color(0xFF222222),
                        Color(0xFF0068C0),
                        Color(0xFFC23934),
                        Color(0xFF27AE60),
                        Color(0xFFF1C40F),
                        Color(0xFF9B59B6)
                    )
                    val backgroundColors = listOf(
                        Color(0xFF1E1E1E),
                        Color(0xFF121212),
                        Color(0xFF263238),
                        Color(0xFF222222),
                        Color(0xFFFFFFFF),
                        Color(0xFFF0F0F0),
                        Color(0xFF0B192C)
                    )
                    val playheadColors = listOf(
                        Color.Red,
                        Color(0xFFFF5252),
                        Color(0xFFF1C40F),
                        Color(0xFF00E5FF),
                        Color(0xFF2ECC71),
                        Color.White
                    )

                    ColorRow(
                        label = "Text Color",
                        colors = textColors,
                        selectedColor = draftStyle.textColor,
                        onSelect = { draftStyle = draftStyle.copy(textColor = it) }
                    )

                    Spacer(modifier = Modifier.height(14.dp))

                    ColorRow(
                        label = "Background Color",
                        colors = backgroundColors,
                        selectedColor = draftStyle.backgroundColor,
                        onSelect = { draftStyle = draftStyle.copy(backgroundColor = it) }
                    )

                    Spacer(modifier = Modifier.height(14.dp))

                    ColorRow(
                        label = "Playhead Marker Color",
                        colors = playheadColors,
                        selectedColor = draftStyle.playheadColor,
                        onSelect = { draftStyle = draftStyle.copy(playheadColor = it) }
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Action Buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    TextButton(onClick = { draftStyle = RythmoPresets.Dark }) {
                        Text("Reset Default")
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    OutlinedButton(onClick = onDismissRequest) {
                        Text("Cancel")
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(onClick = {
                        onSaveStyle(draftStyle)
                        onDismissRequest()
                    }) {
                        Text("Apply Style")
                    }
                }
            }
        }
    }
}

@Composable
private fun ColorRow(
    label: String,
    colors: List<Color>,
    selectedColor: Color,
    onSelect: (Color) -> Unit
) {
    Column {
        Text(
            text = label,
            fontSize = 13.sp,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.onSurface
        )
        Spacer(modifier = Modifier.height(6.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            colors.forEach { color ->
                val isSelected = color == selectedColor
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .clip(CircleShape)
                        .background(color)
                        .border(
                            width = if (isSelected) 3.dp else 1.dp,
                            color = if (isSelected) MaterialTheme.colorScheme.primary else Color.Gray.copy(alpha = 0.5f),
                            shape = CircleShape
                        )
                        .clickable { onSelect(color) }
                )
            }
        }
    }
}
