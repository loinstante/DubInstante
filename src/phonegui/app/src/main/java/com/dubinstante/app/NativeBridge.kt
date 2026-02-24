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
}
