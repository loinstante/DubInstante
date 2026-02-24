package com.dubinstante.app

class NativeBridge {
    companion object {
        init {
            System.loadLibrary("dubinstante_core")
        }
    }

    external fun initialize()
    external fun openVideo(uri: String)
    external fun play()
    external fun pause()
    external fun setVolume(volume: Float)

    external fun setRythmoText(text: String)
    external fun getRythmoText(): String
    external fun setRythmoSpeed(speed: Int)
    external fun getRythmoSpeed(): Int
}
