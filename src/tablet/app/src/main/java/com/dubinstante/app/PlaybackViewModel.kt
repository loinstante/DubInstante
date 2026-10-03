
package com.dubinstante.app

import androidx.compose.ui.graphics.Color
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class PlaybackViewModel : ViewModel() {
    private val nativeBridge = NativeBridge()
    private var engineHandle: Long = 0L

    private val _currentPositionMs = MutableStateFlow(0L)
    val currentPositionMs: StateFlow<Long> = _currentPositionMs.asStateFlow()

    private val _rythmoText = MutableStateFlow("")
    val rythmoText: StateFlow<String> = _rythmoText.asStateFlow()

    private val _rythmoSpeed = MutableStateFlow(100f)
    val rythmoSpeed: StateFlow<Float> = _rythmoSpeed.asStateFlow()

    private val _rythmoStyle = MutableStateFlow(RythmoPresets.Dark)
    val rythmoStyle: StateFlow<RythmoStyle> = _rythmoStyle.asStateFlow()

    // 1 Mic / 1 Piste Take State
    private val _currentTakePath = MutableStateFlow<String?>(null)
    val currentTakePath: StateFlow<String?> = _currentTakePath.asStateFlow()

    private val _takeDurationMs = MutableStateFlow(0L)
    val takeDurationMs: StateFlow<Long> = _takeDurationMs.asStateFlow()

    private val _isAuditionPlaying = MutableStateFlow(false)
    val isAuditionPlaying: StateFlow<Boolean> = _isAuditionPlaying.asStateFlow()

    init {
        engineHandle = nativeBridge.initialize()
        _rythmoText.value = nativeBridge.getRythmoText(engineHandle)
        _rythmoSpeed.value = nativeBridge.getRythmoSpeed(engineHandle).toFloat()

        // Read initial style from C++
        val textCol = nativeBridge.getRythmoTextColor(engineHandle)
        val bgCol = nativeBridge.getRythmoBackgroundColor(engineHandle)
        val textSize = nativeBridge.getRythmoTextSize(engineHandle)
        val playheadCol = nativeBridge.getRythmoPlayheadColor(engineHandle)
        val fontIdx = nativeBridge.getRythmoFontIndex(engineHandle)

        _rythmoStyle.value = RythmoStyle(
            textColor = Color(textCol),
            backgroundColor = Color(bgCol),
            textSizeSp = textSize.toFloat(),
            fontFamilyType = FontFamilyType.fromIndex(fontIdx),
            playheadColor = Color(playheadCol)
        )
    }

    override fun onCleared() {
        super.onCleared()
        if (engineHandle != 0L) {
            nativeBridge.release(engineHandle)
            engineHandle = 0L
        }
    }

    fun openVideo(uri: String) {
        if (engineHandle != 0L) {
            nativeBridge.openVideo(engineHandle, uri)
        }
    }

    fun setRythmoText(text: String) {
        _rythmoText.value = text
        if (engineHandle != 0L) {
            nativeBridge.setRythmoText(engineHandle, text)
        }
    }

    fun setRythmoSpeed(speed: Float) {
        _rythmoSpeed.value = speed
        if (engineHandle != 0L) {
            nativeBridge.setRythmoSpeed(engineHandle, speed.toInt())
        }
    }

    fun setRythmoStyle(style: RythmoStyle) {
        _rythmoStyle.value = style
        if (engineHandle != 0L) {
            nativeBridge.setRythmoStyle(
                handle = engineHandle,
                textColor = style.textArgb,
                bgColor = style.backgroundArgb,
                textSize = style.textSizeSp.toInt(),
                playheadColor = style.playheadArgb,
                fontIndex = style.fontFamilyType.index
            )
        }
    }

    fun setTake(audioPath: String?, durationMs: Long) {
        _currentTakePath.value = audioPath
        _takeDurationMs.value = durationMs
        if (engineHandle != 0L) {
            nativeBridge.setTakeActive(engineHandle, audioPath != null)
            nativeBridge.setTakeDurationMs(engineHandle, durationMs)
        }
    }

    fun clearTake() {
        _currentTakePath.value = null
        _takeDurationMs.value = 0L
        _isAuditionPlaying.value = false
        if (engineHandle != 0L) {
            nativeBridge.setTakeActive(engineHandle, false)
            nativeBridge.setTakeDurationMs(engineHandle, 0L)
        }
    }

    fun setAuditionPlaying(playing: Boolean) {
        _isAuditionPlaying.value = playing
    }

    fun setVolume(volume: Float) {
        if (engineHandle != 0L) {
            nativeBridge.setVolume(engineHandle, volume)
        }
    }

    fun updatePosition(positionMs: Long) {
        _currentPositionMs.value = positionMs
    }
}
