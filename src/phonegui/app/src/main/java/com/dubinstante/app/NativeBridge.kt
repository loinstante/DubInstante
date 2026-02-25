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
}
