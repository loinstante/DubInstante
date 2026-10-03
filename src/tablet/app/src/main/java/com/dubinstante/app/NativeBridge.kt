package com.dubinstante.app

class NativeBridge {
    companion object {
        init {
            System.loadLibrary("dubinstante_core")
        }
    }

    external fun initialize(): Long
    external fun release(handle: Long)
    external fun openVideo(handle: Long, uri: String)
    external fun play(handle: Long)
    external fun pause(handle: Long)
    external fun setVolume(handle: Long, volume: Float)

    external fun setRythmoText(handle: Long, text: String)
    external fun getRythmoText(handle: Long): String
    external fun setRythmoSpeed(handle: Long, speed: Int)
    external fun getRythmoSpeed(handle: Long): Int

    external fun setRythmoStyle(
        handle: Long,
        textColor: Int,
        bgColor: Int,
        textSize: Int,
        playheadColor: Int,
        fontIndex: Int
    )
    external fun getRythmoTextColor(handle: Long): Int
    external fun getRythmoBackgroundColor(handle: Long): Int
    external fun getRythmoTextSize(handle: Long): Int
    external fun getRythmoPlayheadColor(handle: Long): Int
    external fun getRythmoFontIndex(handle: Long): Int

    external fun setTakeActive(handle: Long, active: Boolean)
    external fun isTakeActive(handle: Long): Boolean
    external fun setTakeDurationMs(handle: Long, durationMs: Long)
    external fun getTakeDurationMs(handle: Long): Long
}
