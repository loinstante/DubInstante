
package com.dubinstante.app

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

    init {
        engineHandle = nativeBridge.initialize()
        _rythmoText.value = nativeBridge.getRythmoText(engineHandle)
        _rythmoSpeed.value = nativeBridge.getRythmoSpeed(engineHandle).toFloat()
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

    fun setVolume(volume: Float) {
        if (engineHandle != 0L) {
            nativeBridge.setVolume(engineHandle, volume)
        }
    }

    fun updatePosition(positionMs: Long) {
        _currentPositionMs.value = positionMs
    }
}
