package com.dubinstante.app

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt
import kotlinx.coroutines.delay

@Composable
fun RythmoBand(
    text: String,
    onTextChanged: (String) -> Unit,
    currentPositionMs: Long,
    speedPixelsPerSecond: Int,
    onSeekRequested: (Long) -> Unit,
    modifier: Modifier = Modifier
) {
    val focusRequester = remember { FocusRequester() }
    var isFocused by remember { mutableStateOf(false) }
    val keyboardController = LocalSoftwareKeyboardController.current

    // Fully stateful TextField representation that prevents selection bugs
    var textFieldValue by remember { mutableStateOf(TextFieldValue(text)) }
    LaunchedEffect(text) {
        if (text != textFieldValue.text) {
            textFieldValue = textFieldValue.copy(text = text)
        }
    }

    val currentPositionMsState = rememberUpdatedState(currentPositionMs)
    val speedState = rememberUpdatedState(speedPixelsPerSecond)

    val paint = remember {
        android.graphics.Paint().apply {
            color = android.graphics.Color.WHITE
            textSize = 50f
            isAntiAlias = true
            typeface = android.graphics.Typeface.MONOSPACE
        }
    }
    val charWidth = remember(paint) { paint.measureText("A") }

    var isCursorVisible by remember { mutableStateOf(true) }
    LaunchedEffect(textFieldValue.selection, isFocused) {
        if (isFocused) {
            while (true) {
                isCursorVisible = true
                delay(500)
                isCursorVisible = false
                delay(500)
            }
        } else {
            isCursorVisible = false
        }
    }

    Box(
        modifier = modifier
            .background(Color(0xFF1E1E1E))
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = { tapOffset ->
                        focusRequester.requestFocus()
                        keyboardController?.show()
                        
                        val centerX = size.width / 2f
                        val offsetPixels = (currentPositionMsState.value / 1000f) * speedState.value
                        val startX = centerX - offsetPixels
                        
                        val relativeX = tapOffset.x - startX
                        var charIndex = (relativeX / charWidth).roundToInt()
                        charIndex = charIndex.coerceIn(0, textFieldValue.text.length)
                        
                        textFieldValue = textFieldValue.copy(selection = TextRange(charIndex))
                    }
                )
            }
            .pointerInput(Unit) {
                detectHorizontalDragGestures(
                    onHorizontalDrag = { change, dragAmount ->
                        change.consume()
                        val deltaMs = -(dragAmount / speedState.value) * 1000f
                        val newPos = maxOf(0L, currentPositionMsState.value + deltaMs.toLong())
                        onSeekRequested(newPos)
                    }
                )
            }
    ) {
        BasicTextField(
            value = textFieldValue,
            onValueChange = { newValue ->
                textFieldValue = newValue
                if (newValue.text != text) {
                    onTextChanged(newValue.text)
                }
            },
            modifier = Modifier
                .focusRequester(focusRequester)
                .onFocusChanged { isFocused = it.isFocused }
                .size(1.dp) // Practically invisible 
                .align(Alignment.TopStart),
            textStyle = TextStyle(color = Color.Transparent)
        )

        Canvas(modifier = Modifier.fillMaxSize()) {
            val canvasWidth = size.width
            val canvasHeight = size.height
            val centerX = canvasWidth / 2f

            // Calculate horizontal displacement based on time and speed
            val offsetPixels = (currentPositionMs / 1000f) * speedPixelsPerSecond
            // The first character starts at centerX, and moves left as time goes
            val startX = centerX - offsetPixels

            drawIntoCanvas { canvas ->
                canvas.nativeCanvas.drawText(
                    text,
                    startX,
                    canvasHeight / 2f + 15f,
                    paint
                )
            }

            // Draw custom cursor
            if (isFocused && isCursorVisible && textFieldValue.selection.collapsed) {
                val cursorIndex = textFieldValue.selection.start
                val cursorX = startX + cursorIndex * charWidth
                drawLine(
                    color = Color.White,
                    start = Offset(cursorX, canvasHeight / 2f - 30f),
                    end = Offset(cursorX, canvasHeight / 2f + 20f),
                    strokeWidth = 4f
                )
            }

            // Draw red playhead line
            drawLine(
                color = Color.Red,
                start = Offset(centerX, 0f),
                end = Offset(centerX, canvasHeight),
                strokeWidth = 4f
            )
        }
    }
}
